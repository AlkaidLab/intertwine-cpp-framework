#include "LegacyTransfer.hpp"
#include "fw/Context.hpp"
#include "fw/HttpConstants.hpp"
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <hv/HttpResponseWriter.h>
#include <hv/hthread.h>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <fstream>

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
/** 投递到 writer IO loop 后，事件未开始执行的等待上限 */
static const int kWriterLoopCallTimeoutMs = 5 * 1000; /* 5 s */

struct PostedLegacyEvent {
    hevent_t ev;
    boost::function<void()> fn;

    PostedLegacyEvent() : ev(), fn() {}
};

struct SyncWriterCall {
    boost::mutex mutex;
    boost::condition_variable cv;
    boost::function<void()> fn;
    bool started;
    bool done;

    explicit SyncWriterCall(boost::function<void()> callback)
        : fn(callback), started(false), done(false) {}

private:
    SyncWriterCall(const SyncWriterCall&);
    SyncWriterCall& operator=(const SyncWriterCall&);
};

void postedLegacyEventCb(hevent_t* ev) {
    PostedLegacyEvent* payload =
        static_cast<PostedLegacyEvent*>(hevent_userdata(ev));
    boost::function<void()> fn;
    if (payload) {
        fn = payload->fn;
        delete payload;
    }
    if (fn) fn();
}

bool postToWriterLoop(hv::HttpResponseWriter* writer, boost::function<void()> fn) {
    if (!writer || !writer->io()) return false;
    hloop_t* loop = hevent_loop(writer->io());
    if (!loop) return false;

    PostedLegacyEvent* payload = new PostedLegacyEvent();
    payload->fn = fn;
    payload->ev.event_type = HEVENT_TYPE_CUSTOM;
    payload->ev.cb = postedLegacyEventCb;
    payload->ev.userdata = payload;
    hloop_post_event(loop, &payload->ev);
    return true;
}

bool runOnWriterLoopSync(hv::HttpResponseWriter* writer, boost::function<void()> fn) {
    if (!writer || !writer->io()) return false;
    hloop_t* loop = hevent_loop(writer->io());
    if (!loop) return false;

    if (hloop_tid(loop) == hv_gettid()) {
        if (fn) fn();
        return true;
    }

    boost::shared_ptr<SyncWriterCall> call(new SyncWriterCall(fn));
    if (!postToWriterLoop(writer, [call]() {
            boost::function<void()> localFn;
            {
                boost::lock_guard<boost::mutex> lock(call->mutex);
                call->started = true;
                localFn = call->fn;
            }

            if (localFn) localFn();

            {
                boost::lock_guard<boost::mutex> lock(call->mutex);
                call->fn = boost::function<void()>();
                call->done = true;
            }
            call->cv.notify_one();
        })) {
        return false;
    }

    boost::unique_lock<boost::mutex> lock(call->mutex);
    while (!call->done) {
        if (call->cv.wait_for(
                lock, boost::chrono::milliseconds(kWriterLoopCallTimeoutMs))
            != boost::cv_status::timeout) {
            continue;
        }
        if (!call->started) {
            call->fn = boost::function<void()>();
            call->done = true;
            return false;
        }
    }
    return true;
}

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
    std::string disposition = params.inlineDisposition ? "inline" : "attachment";
    c.setHeader("Content-Disposition",
                disposition + "; filename=\"" + safeFilenameForHeader(params.displayName) + "\"");
    c.setHeader("Accept-Ranges", "bytes");
    c.setContentTypeByFilename(params.displayName.c_str());

    /* ── 小文件路径：全量读入 body ── */
    if (sendLen < kStreamThreshold) {
        if (isRange) {
            std::ifstream ifs(params.physicalPath.c_str(), std::ios::binary);
            if (ifs && ifs.seekg(sendStart) && ifs.good()) {
                std::string rangeBody(static_cast<size_t>(sendLen), '\0');
                ifs.read(&rangeBody[0], sendLen);
                if (ifs.gcount() == sendLen) {
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

    auto ownership = c.writerOwnership();
    hv::HttpResponseWriter* writer =
        static_cast<hv::HttpResponseWriter*>(ownership.get());
    if (!writer || !writer->io() || !hevent_loop(writer->io())) {
        c.error(HttpStatus::InternalError, "internal");
        m_stats.recordEnd(false, startTime);
        if (params.onComplete) params.onComplete(false);
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
    int headerRet = -1;
    if (!runOnWriterLoopSync(writer, [&]() {
            headerRet = c.endHeaders("Content-Length", sendLen);
        }) || headerRet < 0) {
        m_stats.recordEnd(false, startTime);
        if (params.onComplete) params.onComplete(false);
        return;
    }

    /* 分块读取并写入 socket；背压控制 + 断连检测 + 超时 */
    char buf[kStreamChunkSize]; // NOLINT(modernize-avoid-c-arrays)
    int64_t remaining = sendLen;
    bool timedOut = false;
    while (remaining > 0 && ifs.good()) {
        int waitedMs = 0;
        size_t pendingBytes = 0;
        bool connected = false;
        if (!runOnWriterLoopSync(writer, [&]() {
                pendingBytes = c.writeBufsize();
                connected = c.writerConnected();
            })) {
            break;
        }
        while (pendingBytes > kBackpressureThreshold && connected) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(kBackpressureWaitMs));
            waitedMs += kBackpressureWaitMs;
            if (waitedMs >= kBackpressureTimeoutMs) {
                timedOut = true;
                break;
            }
            if (!runOnWriterLoopSync(writer, [&]() {
                    pendingBytes = c.writeBufsize();
                    connected = c.writerConnected();
                })) {
                connected = false;
                break;
            }
        }
        if (timedOut || !connected) break;
        int64_t toRead = remaining < kStreamChunkSize ? remaining : kStreamChunkSize;
        ifs.read(buf, toRead);
        std::streamsize got = ifs.gcount();
        if (got <= 0) break;
        int ret = -1;
        bool connectedAfterWrite = false;
        if (!runOnWriterLoopSync(writer, [&]() {
                ret = c.writeBody(buf, static_cast<int>(got));
                connectedAfterWrite = c.writerConnected();
            })) {
            break;
        }
        if (ret < 0 || !connectedAfterWrite) break;
        remaining -= got;
    }
    (void)runOnWriterLoopSync(writer, [&]() { (void)c.end(); });
    success = (remaining == 0 && !timedOut);
    m_stats.recordEnd(success, startTime);
    if (params.onComplete) params.onComplete(success);
}

} // namespace fw
} // namespace alkaidlab
