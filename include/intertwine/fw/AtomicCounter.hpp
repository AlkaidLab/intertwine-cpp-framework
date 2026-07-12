#ifndef FW_ATOMIC_COUNTER_HPP
#define FW_ATOMIC_COUNTER_HPP
#include <cstdint>
#include <boost/atomic.hpp>

namespace intertwine {
namespace fw {

/**
 * 原子计数器
 * 用于高性能统计计数
 */
class AtomicCounter {
public:
    AtomicCounter();
    explicit AtomicCounter(int64_t initialValue);

    int64_t add(int64_t delta = 1);
    int64_t sub(int64_t delta = 1);
    int64_t get() const;
    void set(int64_t value);
    void reset();

    int64_t operator++();
    int64_t operator++(int);
    int64_t operator--();
    int64_t operator--(int);
    operator int64_t() const;

private:
    boost::atomic<int64_t> m_value;
};

} // namespace fw
} // namespace intertwine

#endif // FW_ATOMIC_COUNTER_HPP
