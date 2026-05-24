// StreamTransfer — 事件驱动文件传输实现
// 参考 libhv defaultLargeFileHandler 的 onwrite 回调模式：
//   writer->onwrite = pump;  writer->EndHeaders();
//   pump() 在 EPOLLOUT 事件中读取磁盘块 → WriteBody → 直到 remaining == 0 → End()

#include "StreamTransfer.hpp"
#include "fw/Context.hpp"
#include "fw/HttpConstants.hpp"
#include <hv/HttpResponseWriter.h>
#include <fstream>
#include <memory>

namespace alkaidlab {
namespace fw {

namespace {

static const int kChunkSize = 256 * 1024; /* 256 KB */

/** 异步传输状态，由 shared_ptr 管理生命周期。
 *  onwrite 回调 capture shared_ptr<TransferState> 确保对象不会在传输完成前被析构。 */
struct TransferState {
    std::ifstream file;
    int64_t remaining;
    hv::HttpResponseWriter* writer;      // 原始指针，用于 WriteBody/End/isConnected
    std::shared_ptr<void> ownership;     // 共享所有权，保持 writer 存活
    boost::function<void(bool)> onComplete;
    TransferStats* stats;
    boost::chrono::steady_clock::time_point startTime;
    bool finished;

    TransferState() : remaining(0), writer(0), stats(0), finished(false) {}
    ~TransferState() {
        // 安全网：异常析构时确保回调被调用
        if (!finished) {
            if (stats) stats->recordEnd(false, startTime);
            if (onComplete) onComplete(false);
        }
    }

private:
    TransferState(const TransferState&);
    TransferState& operator=(const TransferState&);
};

/** RFC 2616 safe filename for Content-Disposition (ASCII only) */
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

void finish(std::shared_ptr<TransferState>& s, bool success) {
    if (s->finished) return;
    s->finished = true;
    s->file.close();
    s->writer->onwrite = nullptr;  // 打破循环引用
    if (s->stats) s->stats->recordEnd(success, s->startTime);
    if (s->onComplete) s->onComplete(success);
}

/** onwrite 回调：EPOLLOUT 触发时读取下一块并发送 */
void pump(std::shared_ptr<TransferState> s) {
    if (s->finished) { return; }             // 防止 finish 后重入
    if (!s->writer->isConnected()) {
        finish(s, false);
        return;
    }
    // 仅在写缓冲区完全排空时发送下一块，避免积压
    if (!s->writer->isWriteComplete()) return;

    char buf[kChunkSize]; // NOLINT(modernize-avoid-c-arrays)
    int64_t toRead = s->remaining < kChunkSize ? s->remaining : kChunkSize;
    s->file.read(buf, toRead);
    std::streamsize got = s->file.gcount();
    if (got <= 0) {
        finish(s, false);
        return;
    }

    // 先扣减 remaining，再写数据。
    // WriteBody 可能同步触发 on_write → 重入 pump()；
    // 如果 remaining 在 WriteBody 之后才扣减，
    // 重入的 pump 看到旧 remaining → 读到 EOF → finish(false)。
    s->remaining -= got;

    if (s->remaining <= 0) {
        // 最后一块：清除 onwrite 防止 WriteBody 的同步回调重入
        s->writer->onwrite = nullptr;
    }

    int ret = s->writer->WriteBody(buf, static_cast<int>(got));
    if (ret < 0) {
        finish(s, false);
        return;
    }

    if (s->remaining <= 0) {
        s->writer->End();
        finish(s, true);
    }
    // 否则等待下一次 EPOLLOUT → onwrite → pump
}

} // namespace

void StreamTransfer::send(Context& c, const TransferParams& params) {
    bool isRange = (params.rangeStart >= 0 && params.rangeEnd >= 0);
    int64_t sendStart = isRange ? params.rangeStart : 0;
    int64_t sendLen   = isRange ? (params.rangeEnd - params.rangeStart + 1)
                                : params.fileSize;

    /* 获取 writer 共享所有权 */
    std::shared_ptr<void> ownership = c.writerOwnership();
    if (!ownership) {
        c.error(HttpStatus::InternalError, "internal");
        if (params.onComplete) params.onComplete(false);
        return;
    }
    hv::HttpResponseWriter* writer =
        static_cast<hv::HttpResponseWriter*>(ownership.get());

    /* 创建传输状态 */
    std::shared_ptr<TransferState> state = std::make_shared<TransferState>();
    state->file.open(params.physicalPath.c_str(), std::ios::binary);
    if (!state->file) {
        c.error(HttpStatus::InternalError, "internal");
        if (params.onComplete) params.onComplete(false);
        return;
    }
    if (isRange && !state->file.seekg(sendStart)) {
        c.error(HttpStatus::InternalError, "internal");
        if (params.onComplete) params.onComplete(false);
        return;
    }
    state->remaining   = sendLen;
    state->writer      = writer;
    state->ownership   = ownership;
    state->onComplete  = params.onComplete;
    state->stats       = &m_stats;

    state->startTime = m_stats.recordStart(sendLen);

    /* 设置响应头（Context 仍存活） */
    c.setHeader("Content-Disposition",
                "attachment; filename=\"" + safeFilenameForHeader(params.displayName) + "\"");
    c.setHeader("Accept-Ranges", "bytes");
    c.setContentTypeByFilename(params.displayName.c_str());

    if (isRange) {
        c.setStatus(HttpStatus::PartialContent);
        c.setHeader("Content-Range", "bytes "
            + std::to_string(sendStart) + "-"
            + std::to_string(params.rangeEnd) + "/"
            + std::to_string(params.fileSize));
    } else {
        c.setStatus(HttpStatus::Ok);
    }

    /* 注册 onwrite 回调 — 必须在 endHeaders 之前 */
    writer->onwrite = [state](hv::Buffer*) { pump(state); };

    /* 零长度文件：直接完成，不进入状态机 */
    if (sendLen == 0) {
        c.endHeaders("Content-Length", static_cast<int64_t>(0));
        c.end();
        finish(state, true);
        return;
    }

    /* 发送响应头 → 写完成事件 → pump() 开始发送数据 */
    c.endHeaders("Content-Length", sendLen);

    /* 接管响应生命周期：阻止 fw 异步分发器在 handler 返回后立即调用 writer->End()。
     * 对 Connection: close 请求，过早 End() 会触发 socket close，
     * 导致后续 EPOLLOUT-driven pump() 看到 isConnected()=false。 */
    c.markStreamingHandoff();

    /* send() 立即返回，IO 线程驱动后续 pump */
}

} // namespace fw
} // namespace alkaidlab
