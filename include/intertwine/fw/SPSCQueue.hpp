#ifndef FW_SPSC_QUEUE_HPP
#define FW_SPSC_QUEUE_HPP

#include <boost/atomic.hpp>
#include <cstdint>
#include <cstring>
#include <memory>

namespace intertwine {
namespace fw {

/**
 * 单生产者单消费者无锁队列（SPSC）
 * 自实现环形缓冲区，性能优于MPMC队列
 * 使用内存屏障保证线程安全
 */
template<typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity);
    ~SPSCQueue();

    bool push(const T& item);
    bool push(T&& item);
    bool pop(T& item);
    bool empty() const;
    size_t size() const;
    size_t capacity() const;
    void clear();

private:
    T* m_buffer;
    size_t m_capacity;
    size_t m_mask;

    boost::atomic<size_t> m_writePos;
    boost::atomic<size_t> m_readPos;

    boost::atomic<uint64_t> m_pushCount;
    boost::atomic<uint64_t> m_popCount;
    boost::atomic<uint64_t> m_dropCount;

    size_t nextPowerOfTwo(size_t n) const;
    bool isPowerOfTwo(size_t n) const;
};

} // namespace fw
} // namespace intertwine

#include "intertwine/fw/SPSCQueue.inl"

#endif // FW_SPSC_QUEUE_HPP
