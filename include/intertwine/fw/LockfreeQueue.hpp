#ifndef FW_LOCKFREE_QUEUE_HPP
#define FW_LOCKFREE_QUEUE_HPP

#include <boost/lockfree/queue.hpp>
#include <boost/atomic.hpp>
#include <cstdint>
#include <memory>

namespace intertwine {
namespace fw {

/**
 * 无锁队列封装（MPMC场景）
 * 基于boost::lockfree::queue实现
 * 支持多生产者多消费者
 */
template<typename T>
class LockfreeQueue {
public:
    explicit LockfreeQueue(size_t capacity);
    ~LockfreeQueue();

    bool push(const T& item);
    bool push(T&& item);
    bool pop(T& item);
    bool empty() const;
    size_t size() const;
    size_t capacity() const;
    void clear();

private:
    boost::lockfree::queue<T>* m_queue;
    size_t m_capacity;
    boost::atomic<uint64_t> m_pushCount;
    boost::atomic<uint64_t> m_popCount;
    boost::atomic<uint64_t> m_dropCount;
};

} // namespace fw
} // namespace intertwine

#include "intertwine/fw/LockfreeQueue.inl"

#endif // FW_LOCKFREE_QUEUE_HPP
