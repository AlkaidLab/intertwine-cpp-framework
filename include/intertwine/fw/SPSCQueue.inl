#ifndef FW_SPSC_QUEUE_INL
#define FW_SPSC_QUEUE_INL

#include "intertwine/fw/SPSCQueue.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>

namespace intertwine {
namespace fw {

template<typename T>
SPSCQueue<T>::SPSCQueue(size_t capacity)
    : m_capacity(0)
    , m_mask(0)
    , m_writePos(0)
    , m_readPos(0)
    , m_pushCount(0)
    , m_popCount(0)
    , m_dropCount(0)
{
    m_capacity = nextPowerOfTwo(capacity);
    m_mask = m_capacity - 1;
    m_buffer = new T[m_capacity];
}

template<typename T>
SPSCQueue<T>::~SPSCQueue() {
    if (m_buffer) {
        delete[] m_buffer;
    }
}

template<typename T>
bool SPSCQueue<T>::push(const T& item) {
    size_t currentWrite = m_writePos.load(boost::memory_order_relaxed);
    size_t nextWrite = (currentWrite + 1) & m_mask;
    size_t currentRead = m_readPos.load(boost::memory_order_acquire);

    if (nextWrite == currentRead) {
        m_dropCount.fetch_add(1, boost::memory_order_relaxed);
        return false;
    }

    m_buffer[currentWrite] = item;
    m_writePos.store(nextWrite, boost::memory_order_release);
    m_pushCount.fetch_add(1, boost::memory_order_relaxed);
    return true;
}

template<typename T>
bool SPSCQueue<T>::push(T&& item) {
    size_t currentWrite = m_writePos.load(boost::memory_order_relaxed);
    size_t nextWrite = (currentWrite + 1) & m_mask;
    size_t currentRead = m_readPos.load(boost::memory_order_acquire);

    if (nextWrite == currentRead) {
        m_dropCount.fetch_add(1, boost::memory_order_relaxed);
        return false;
    }

    m_buffer[currentWrite] = std::move(item);
    m_writePos.store(nextWrite, boost::memory_order_release);
    m_pushCount.fetch_add(1, boost::memory_order_relaxed);
    return true;
}

template<typename T>
bool SPSCQueue<T>::pop(T& item) {
    size_t currentRead = m_readPos.load(boost::memory_order_relaxed);
    size_t currentWrite = m_writePos.load(boost::memory_order_acquire);

    if (currentRead == currentWrite) {
        return false;
    }

    item = std::move(m_buffer[currentRead]);
    size_t nextRead = (currentRead + 1) & m_mask;
    m_readPos.store(nextRead, boost::memory_order_release);
    m_popCount.fetch_add(1, boost::memory_order_relaxed);
    return true;
}

template<typename T>
bool SPSCQueue<T>::empty() const {
    size_t currentRead = m_readPos.load(boost::memory_order_acquire);
    size_t currentWrite = m_writePos.load(boost::memory_order_acquire);
    return currentRead == currentWrite;
}

template<typename T>
size_t SPSCQueue<T>::size() const {
    size_t currentRead = m_readPos.load(boost::memory_order_acquire);
    size_t currentWrite = m_writePos.load(boost::memory_order_acquire);

    if (currentWrite >= currentRead) {
        return currentWrite - currentRead;
    } else {
        return (m_capacity - currentRead) + currentWrite;
    }
}

template<typename T>
size_t SPSCQueue<T>::capacity() const {
    return m_capacity;
}

template<typename T>
void SPSCQueue<T>::clear() {
    T item;
    while (pop(item)) {
    }
}

template<typename T>
size_t SPSCQueue<T>::nextPowerOfTwo(size_t n) const {
    if (n == 0) {
        return 1;
    }
    if (isPowerOfTwo(n)) {
        return n;
    }
    size_t result = 1;
    while (result < n) {
        result <<= 1;
    }
    return result;
}

template<typename T>
bool SPSCQueue<T>::isPowerOfTwo(size_t n) const {
    return (n != 0) && ((n & (n - 1)) == 0);
}

} // namespace fw
} // namespace intertwine

#endif // FW_SPSC_QUEUE_INL
