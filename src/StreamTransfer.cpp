// StreamTransfer — 事件驱动文件传输实现
// 参考 libhv defaultLargeFileHandler 的 onwrite 回调模式：
//   writer->onwrite = pump;  writer->EndHeaders();
//   pump() 在 EPOLLOUT 事件中读取磁盘块 → WriteBody → 直到 remaining == 0 → End()

#include "StreamTransfer.hpp"
#include "intertwine/fw/Context.hpp"
#include "intertwine/fw/HttpConstants.hpp"
#include <boost/function.hpp>
#include <hv/HttpResponseWriter.h>
#include <fstream>
#include <memory>

namespace intertwine {
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
    bool pumping;
    bool cleanupScheduled;

    TransferState()
        : remaining(0), writer(0), stats(0), finished(false),
          pumping(false), cleanupScheduled(false) {}
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

struct PostedTransferEvent {
    hevent_t ev;
    boost::function<void()> fn;

    PostedTransferEvent() : ev(), fn() {}
};

void postedTransferEventCb(hevent_t* ev) {
    PostedTransferEvent* payload =
        static_cast<PostedTransferEvent*>(hevent_userdata(ev));
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

    PostedTransferEvent* payload = new PostedTransferEvent();
    payload->fn = fn;
    payload->ev.event_type = HEVENT_TYPE_CUSTOM;
    payload->ev.cb = postedTransferEventCb;
    payload->ev.userdata = payload;
    hloop_post_event(loop, &payload->ev);
    return true;
}

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

void scheduleCleanup(const std::shared_ptr<TransferState>& s) {
    if (!s || s->cleanupScheduled) return;
    s->cleanupScheduled = true;

    if (!postToWriterLoop(s->writer, [s]() {
            if (s->writer) {
                s->writer->onwrite = nullptr;
                s->writer->onclose = nullptr;
            }
            s->ownership.reset();
        })) {
        s->ownership.reset();
    }
}

void finish(std::shared_ptr<TransferState> s, bool success) {
    if (s->finished) return;
    s->finished = true;
    s->file.close();
    if (s->stats) s->stats->recordEnd(success, s->startTime);
    if (s->onComplete) s->onComplete(success);
    scheduleCleanup(s);
}

/** onwrite 回调：EPOLLOUT 触发时读取下一块并发送 */
void pump(std::shared_ptr<TransferState> s) {
    if (s->finished) { return; }             // 防止 finish 后重入
    if (s->pumping) {
        return;
    }
    s->pumping = true;

    while (!s->finished) {
        if (!s->writer->isConnected()) {
            finish(s, false);
            break;
        }
        // 仅在写缓冲区完全排空时发送下一块，避免积压
        if (!s->writer->isWriteComplete()) break;

        char buf[kChunkSize]; // NOLINT(modernize-avoid-c-arrays)
        int64_t toRead = s->remaining < kChunkSize ? s->remaining : kChunkSize;
        s->file.read(buf, toRead);
        std::streamsize got = s->file.gcount();
        if (got <= 0) {
            finish(s, false);
            break;
        }

        // 先扣减 remaining，再写数据。
        // WriteBody 可能同步触发 on_write；pumping 标记会阻止递归重入，
        // 当前循环在 WriteBody 返回后继续判断是否还能发送下一块。
        s->remaining -= got;

        int ret = s->writer->WriteBody(buf, static_cast<int>(got));
        if (ret < 0) {
            finish(s, false);
            break;
        }

        if (s->remaining <= 0) {
            s->writer->End();
            finish(s, true);
            break;
        }
    }

    s->pumping = false;
    // 否则等待下一次 EPOLLOUT → onwrite → pump
}

void startStreaming(std::shared_ptr<TransferState> state, int64_t sendLen) {
    if (state->finished) return;
    if (!state->writer->isConnected()) {
        finish(state, false);
        return;
    }

    state->writer->onwrite = [state](hv::Buffer*) { pump(state); };
    state->writer->onclose = [state]() { finish(state, false); };

    int ret = state->writer->EndHeaders("Content-Length", sendLen);
    if (ret < 0) {
        finish(state, false);
        return;
    }
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
    if (!writer->io() || !hevent_loop(writer->io())) {
        c.error(HttpStatus::InternalError, "internal");
        if (params.onComplete) params.onComplete(false);
        return;
    }

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
    std::string disposition = params.inlineDisposition ? "inline" : "attachment";
    c.setHeader("Content-Disposition",
                disposition + "; filename=\"" + safeFilenameForHeader(params.displayName) + "\"");
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

    /* 接管响应生命周期必须早于投递异步发送。
     * 后续 EndHeaders/WriteBody 均在 writer 所属 IO loop 执行，
     * 避免工作线程同步写 socket 导致 onwrite 重入和生命周期竞争。 */
    c.markStreamingHandoff();

    /* 零长度文件：直接完成，不进入状态机 */
    if (sendLen == 0) {
        if (!postToWriterLoop(writer, [state]() {
                if (!state->writer->isConnected()) {
                    finish(state, false);
                    return;
                }
                state->writer->EndHeaders("Content-Length", static_cast<int64_t>(0));
                state->writer->End();
                finish(state, true);
            })) {
            finish(state, false);
        }
        return;
    }

    /* 发送响应头 → 写完成事件 → pump() 开始发送数据 */
    if (!postToWriterLoop(writer, [state, sendLen]() {
            startStreaming(state, sendLen);
        })) {
        finish(state, false);
        return;
    }

    /* send() 立即返回，IO 线程驱动后续 pump */
}

} // namespace fw
} // namespace intertwine
