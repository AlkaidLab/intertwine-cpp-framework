#ifndef FW_FLOW_CONTROLLER_HPP
#define FW_FLOW_CONTROLLER_HPP

#include <cstdint>
#include <boost/atomic.hpp>
#include <boost/chrono.hpp>

namespace intertwine {
namespace fw {

/**
 * 流控制器
 * 用于控制数据采集，防止队列溢出
 */
class FlowController {
public:
    FlowController(size_t maxQueueSize, bool dropOnFull = true);
    ~FlowController();

    bool canPush(size_t currentSize) const;
    bool tryPush(size_t currentSize);
    void recordDrop();
    uint64_t getDropCount() const;
    void resetDropCount();
    size_t getMaxQueueSize() const;
    void setMaxQueueSize(size_t maxSize);
    bool isDropOnFull() const;
    void setDropOnFull(bool dropOnFull);

private:
    size_t m_maxQueueSize;
    bool m_dropOnFull;
    boost::atomic<uint64_t> m_dropCount;
    boost::atomic<uint64_t> m_totalPushAttempts;
    boost::atomic<uint64_t> m_totalDrops;
};

} // namespace fw
} // namespace intertwine

#endif // FW_FLOW_CONTROLLER_HPP
