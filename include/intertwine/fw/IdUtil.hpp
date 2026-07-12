#ifndef INTERTWINE_CPP_FRAMEWORK_ID_UTIL_HPP
#define INTERTWINE_CPP_FRAMEWORK_ID_UTIL_HPP

#include <string>
#include <cstdint>

namespace intertwine {
namespace fw {

class SnowflakeGenerator {
public:
    SnowflakeGenerator(int64_t datacenterId = 0, int64_t workerId = 0);
    int64_t nextId();
    std::string nextIdStr();

private:
    int64_t currentTimeMillis();

    static const int64_t EPOCH = 946684800000LL;
    static const int64_t SEQUENCE_BITS = 12;
    static const int64_t WORKER_ID_BITS = 5;
    static const int64_t DATACENTER_ID_BITS = 5;
    static const int64_t MAX_SEQUENCE = (1LL << SEQUENCE_BITS) - 1;
    static const int64_t MAX_WORKER_ID = (1LL << WORKER_ID_BITS) - 1;
    static const int64_t MAX_DATACENTER_ID = (1LL << DATACENTER_ID_BITS) - 1;
    static const int64_t WORKER_ID_SHIFT = SEQUENCE_BITS;
    static const int64_t DATACENTER_ID_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS;
    static const int64_t TIMESTAMP_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;

    int64_t m_datacenterId;
    int64_t m_workerId;
    int64_t m_sequence;
    int64_t m_lastTimestamp;
};

class IdUtil {
public:
    static std::string generateV4();
    static std::string generateTimedV4();
    static int64_t generateSnowflakeId();
    static std::string generateSnowflakeIdStr();
    static void initSnowflake(int64_t datacenterId, int64_t workerId);
};

inline bool isSafeId(const std::string& s) {
    if (s.empty() || s.size() > 64) {
        return false;
    }
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        // NOLINTNEXTLINE(readability-simplify-boolean-expr)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_ID_UTIL_HPP
