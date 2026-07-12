#ifndef FW_LOCKFREE_QUEUE_INL
#define FW_LOCKFREE_QUEUE_INL

#include "intertwine/fw/LockfreeQueue.hpp"
#include <algorithm>
#include <cassert>

namespace intertwine {
namespace fw {

template<typename T>
LockfreeQueue<T>::LockfreeQueue(size_t capacity)
    : m_capacity(capacity)
    , m_pushCount(0)
    , m_popCount(0)
    , m_dropCount(0)
{
    size_t actualCapacity = 1;
    while (actualCapacity < capacity) {
        actualCapacity <<= 1;
    }
    m_capacity = actualCapacity;
    m_queue = new boost::lockfree::queue<T>(m_capacity);
}

template<typename T>
LockfreeQueue<T>::~LockfreeQueue() {
    delete m_queue;
}

template<typename T>
bool LockfreeQueue<T>::push(const T& item) {
    bool success = m_queue->push(item);
    if (success) {
        m_pushCount.fetch_add(1, boost::memory_order_relaxed);
    } else {
        m_dropCount.fetch_add(1, boost::memory_order_relaxed);
    }
    return success;
}

template<typename T>
bool LockfreeQueue<T>::push(T&& item) {
    bool success = m_queue->push(std::move(item));
    if (success) {
        m_pushCount.fetch_add(1, boost::memory_order_relaxed);
    } else {
        m_dropCount.fetch_add(1, boost::memory_order_relaxed);
    }
    return success;
}

template<typename T>
bool LockfreeQueue<T>::pop(T& item) {
    bool success = m_queue->pop(item);
    if (success) {
        m_popCount.fetch_add(1, boost::memory_order_relaxed);
    }
    return success;
}

template<typename T>
bool LockfreeQueue<T>::empty() const {
    return m_queue->empty();
}

template<typename T>
size_t LockfreeQueue<T>::size() const {
    uint64_t pushCount = m_pushCount.load(boost::memory_order_relaxed);
    uint64_t popCount = m_popCount.load(boost::memory_order_relaxed);
    uint64_t sz = (pushCount > popCount) ? (pushCount - popCount) : 0;
    return static_cast<size_t>(sz);
}

template<typename T>
size_t LockfreeQueue<T>::capacity() const {
    return m_capacity;
}

template<typename T>
void LockfreeQueue<T>::clear() {
    T item;
    while (pop(item)) {
    }
}

} // namespace fw
} // namespace intertwine

#endif // FW_LOCKFREE_QUEUE_INL
