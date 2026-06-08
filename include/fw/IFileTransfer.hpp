#ifndef ALKAIDLAB_FW_I_FILE_TRANSFER_HPP
#define ALKAIDLAB_FW_I_FILE_TRANSFER_HPP

#include <string>
#include <cstdint>
#include <memory>
#include <boost/function.hpp>
#include <boost/atomic.hpp>
#include <boost/chrono.hpp>

namespace alkaidlab {
namespace fw {

class Context;

/** 传输统计（所有策略共享，原子操作线程安全） */
struct TransferStats {
    boost::atomic<int64_t> totalCount;       // 累计传输次数
    boost::atomic<int64_t> totalBytes;       // 累计传输字节数
    boost::atomic<int64_t> activeCount;      // 当前活跃传输数
    boost::atomic<int64_t> errorCount;       // 累计错误次数
    boost::atomic<int64_t> totalDurationMs;  // 累计传输耗时（毫秒）

    TransferStats()
        : totalCount(0), totalBytes(0), activeCount(0)
        , errorCount(0), totalDurationMs(0) {}

    /** 记录传输开始，返回时间戳用于 recordEnd */
    boost::chrono::steady_clock::time_point recordStart(int64_t bytes) {
        ++totalCount;
        totalBytes.fetch_add(bytes);
        ++activeCount;
        return boost::chrono::steady_clock::now();
    }
    void recordEnd(bool success, const boost::chrono::steady_clock::time_point& startTime) {
        --activeCount;
        if (!success) ++errorCount;
        int64_t ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(
            boost::chrono::steady_clock::now() - startTime).count();
        totalDurationMs.fetch_add(ms);
    }
};

/** 下载传输参数（Handler → Strategy 传递） */
struct TransferParams {
    std::string physicalPath;
    std::string displayName;     // Content-Disposition + MIME 推导
    bool inlineDisposition;      // true = inline preview, false = attachment download
    int64_t fileSize;
    int64_t rangeStart;          // Range 起始（-1 = 无 Range）
    int64_t rangeEnd;            // Range 结束（-1 = 无 Range）
    /** 可选：传输完成回调（success=true 正常完成，false=中断/错误）
     *  Legacy 策略在 send() 返回前同步调用；
     *  Stream/Accel 策略在 IO 线程异步调用。 */
    boost::function<void(bool success)> onComplete;

    TransferParams()
        : inlineDisposition(false), fileSize(0), rangeStart(-1), rangeEnd(-1) {}
};

/** 服务端文件传输策略接口（与 ITransport 客户端传输对称） */
class IFileTransfer {
public:
    virtual ~IFileTransfer() {}

    virtual void send(Context& c, const TransferParams& params) = 0;

    /** 策略名称（日志/调试用） */
    virtual const char* name() const = 0;

    /** 传输统计（引用同一实例，线程安全读取） */
    TransferStats& stats() { return m_stats; }
    const TransferStats& stats() const { return m_stats; }

protected:
    TransferStats m_stats;
};

/** 工厂：根据模式创建传输策略（与 TransportFactory 模式一致） */
class FileTransferFactory {
public:
    /**
     * @param mode "legacy" / "stream" / "accel"
     * @param accelPrefix  仅 accel 模式有效（如 "/internal/files/"）
     * @return 策略实例，失败返回 nullptr
     */
    static std::unique_ptr<IFileTransfer> create(
        const std::string& mode,
        const std::string& accelPrefix = "");
};

} // namespace fw
} // namespace alkaidlab

#endif
