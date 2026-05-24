#include "LegacyTransfer.hpp"
#include "fw/Context.hpp"
#include "fw/HttpConstants.hpp"
#include "fw/Logger.hpp"
#include <fstream>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

namespace alkaidlab {
namespace fw {

namespace {

/** 流式发送阈值：文件大于此值时使用 writer 分块发送以避免全量内存占用 */
static const int64_t kStreamThreshold = 4LL * 1024 * 1024; /* 4 MB */
/** 流式发送每块大小 */
static const int kStreamChunkSize = 256 * 1024; /* 256 KB */
/** 背压阈值：写缓冲区超过此值时暂停磁盘读取，等待 socket 排空 */
static const size_t kBackpressureThreshold = 4UL * 1024 * 1024; /* 4 MB */
/** 背压等待间隔 */
static const int kBackpressureWaitMs = 10; /* 10 ms */
/** 背压超时：超过此时间仍未排空则放弃发送 */
static const int kBackpressureTimeoutMs = 60 * 1000; /* 60 s */

/** RFC 2616 safe filename for Content-Disposition (ASCII only, replace others) */
std::string safeFilenameForHeader(const std::string& name) {
    std::string safe;
    safe.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        char ch = name[i];
        if (ch >= 0x20 && ch < 0x7F && ch != '"' && ch != '\\') {
            safe += ch;
        } else {
            safe += '_';
        }
    }
    return safe;
}

} // namespace

void LegacyTransfer::send(Context& c, const TransferParams& params) {
    bool isRange = (params.rangeStart >= 0 && params.rangeEnd >= 0);
    int64_t sendStart = isRange ? params.rangeStart : 0;
    int64_t sendLen   = isRange ? (params.rangeEnd - params.rangeStart + 1)
                                : params.fileSize;
    bool success = false;
    auto startTime = m_stats.recordStart(sendLen);

    /* ── Headers ── */
    c.setHeader("Content-Disposition",
                "attachment; filename=\"" + safeFilenameForHeader(params.displayName) + "\"");
    c.setHeader("Accept-Ranges", "bytes");
    c.setContentTypeByFilename(params.displayName.c_str());

    /* ── 小文件路径：全量读入 body ── */
    if (sendLen < kStreamThreshold) {
        if (isRange) {
            std::ifstream ifs(params.physicalPath.c_str(), std::ios::binary);
            bool openOk = static_cast<bool>(ifs);
            bool seekOk = false;
            bool goodAfterSeek = false;
            int64_t gotBytes = -1;
            if (openOk) {
                seekOk = static_cast<bool>(ifs.seekg(sendStart));
                goodAfterSeek = ifs.good();
            }
            if (openOk && seekOk && goodAfterSeek) {
                std::string rangeBody(static_cast<size_t>(sendLen), '\0');
                ifs.read(&rangeBody[0], sendLen);
                gotBytes = static_cast<int64_t>(ifs.gcount());
                if (gotBytes == sendLen) {
                    c.setStatus(HttpStatus::PartialContent);
                    c.setHeader("Content-Range", "bytes "
                        + std::to_string(sendStart) + "-"
                        + std::to_string(params.rangeEnd) + "/"
                        + std::to_string(params.fileSize));
                    c.setBody(rangeBody);
                    success = true;
                }
            }
            if (!success) {
                /* 临时调试：定位 via 浏览器 Range 下载失败的具体阶段 */
                LOG_WARN("[legacy.range.debug] FAILED path=" + params.physicalPath
                         + " sendStart=" + std::to_string(sendStart)
                         + " sendLen=" + std::to_string(sendLen)
                         + " fileSize=" + std::to_string(params.fileSize)
                         + " openOk=" + (openOk ? "1" : "0")
                         + " seekOk=" + (seekOk ? "1" : "0")
                         + " goodAfterSeek=" + (goodAfterSeek ? "1" : "0")
                         + " gotBytes=" + std::to_string(gotBytes));
                c.error(HttpStatus::InternalError, "internal");
            }
        } else {
            c.serveFile(params.physicalPath.c_str());
            c.setContentTypeByFilename(params.displayName.c_str());
            success = true;
        }
        m_stats.recordEnd(success, startTime);
        if (params.onComplete) params.onComplete(success);
        return;
    }

    /* ── 大文件路径：通过 writer 流式发送 ── */
    if (!c.hasWriter()) {
        /* 无 writer 回退 */
        if (isRange) {
            std::string fallbackBody(static_cast<size_t>(sendLen), '\0');
            std::ifstream ifs(params.physicalPath.c_str(), std::ios::binary);
            if (ifs) { ifs.seekg(sendStart); ifs.read(&fallbackBody[0], sendLen); }
            c.setBody(fallbackBody);
            c.setStatus(isRange ? HttpStatus::PartialContent : HttpStatus::Ok);
        } else {
            c.serveFile(params.physicalPath.c_str());
            c.setContentTypeByFilename(params.displayName.c_str());
        }
        m_stats.recordEnd(true, startTime);
        if (params.onComplete) params.onComplete(true);
        return;
    }

    std::ifstream ifs(params.physicalPath.c_str(), std::ios::binary);
    if (!ifs) {
        c.error(HttpStatus::InternalError, "internal");
        m_stats.recordEnd(false, startTime);
        if (params.onComplete) params.onComplete(false);
        return;
    }
    if (isRange && !ifs.seekg(sendStart)) {
        c.error(HttpStatus::InternalError, "internal");
        m_stats.recordEnd(false, startTime);
        if (params.onComplete) params.onComplete(false);
        return;
    }

    if (isRange) {
        c.setStatus(HttpStatus::PartialContent);
        c.setHeader("Content-Range", "bytes "
            + std::to_string(sendStart) + "-"
            + std::to_string(params.rangeEnd) + "/"
            + std::to_string(params.fileSize));
    } else {
        c.setStatus(HttpStatus::Ok);
    }
    c.endHeaders("Content-Length", sendLen);

    /* 分块读取并写入 socket；背压控制 + 断连检测 + 超时 */
    char buf[kStreamChunkSize]; // NOLINT(modernize-avoid-c-arrays)
    int64_t remaining = sendLen;
    bool timedOut = false;
    while (remaining > 0 && ifs.good()) {
        int waitedMs = 0;
        while (c.writeBufsize() > kBackpressureThreshold && c.writerConnected()) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(kBackpressureWaitMs));
            waitedMs += kBackpressureWaitMs;
            if (waitedMs >= kBackpressureTimeoutMs) {
                timedOut = true;
                break;
            }
        }
        if (timedOut || !c.writerConnected()) break;
        int64_t toRead = remaining < kStreamChunkSize ? remaining : kStreamChunkSize;
        ifs.read(buf, toRead);
        std::streamsize got = ifs.gcount();
        if (got <= 0) break;
        int ret = c.writeBody(buf, static_cast<int>(got));
        if (ret < 0 || !c.writerConnected()) break;
        remaining -= got;
    }
    c.end();
    success = (remaining == 0 && !timedOut);
    m_stats.recordEnd(success, startTime);
    if (params.onComplete) params.onComplete(success);
}

} // namespace fw
} // namespace alkaidlab
