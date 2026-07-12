#include "intertwine/fw/AtomicCounter.hpp"

namespace intertwine {
namespace fw {

AtomicCounter::AtomicCounter()
    : m_value(0)
{
}

AtomicCounter::AtomicCounter(int64_t initialValue)
    : m_value(initialValue)
{
}

int64_t AtomicCounter::add(int64_t delta) {
    return m_value.fetch_add(delta, boost::memory_order_relaxed) + delta;
}

int64_t AtomicCounter::sub(int64_t delta) {
    return m_value.fetch_sub(delta, boost::memory_order_relaxed) - delta;
}

int64_t AtomicCounter::get() const {
    return m_value.load(boost::memory_order_relaxed);
}

void AtomicCounter::set(int64_t value) {
    m_value.store(value, boost::memory_order_relaxed);
}

void AtomicCounter::reset() {
    m_value.store(0, boost::memory_order_relaxed);
}

int64_t AtomicCounter::operator++() {
    return add(1);
}

int64_t AtomicCounter::operator++(int) {
    return add(1) - 1;
}

int64_t AtomicCounter::operator--() {
    return sub(1);
}

int64_t AtomicCounter::operator--(int) {
    return sub(1) + 1;
}

AtomicCounter::operator int64_t() const {
    return get();
}

} // namespace fw
} // namespace intertwine
