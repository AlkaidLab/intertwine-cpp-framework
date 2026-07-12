#include "intertwine/fw/TimeUtil.hpp"
#include <gtest/gtest.h>
#include <boost/chrono.hpp>
#include <string>
#include <ctime>

namespace intertwine {
namespace fw {

TEST(TimeUtil, NowISO8601NonEmpty) {
    std::string s = TimeUtil::nowISO8601();
    EXPECT_FALSE(s.empty());
    EXPECT_GE(s.size(), 20u);
}

TEST(TimeUtil, ToISO8601FromTimestamp) {
    time_t t = 946684800;
    std::string s = TimeUtil::toISO8601(t, 0);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(s.find("2000"), std::string::npos);
    EXPECT_NE(s.find("01"), std::string::npos);
}

TEST(TimeUtil, DurationMs) {
    auto start = boost::chrono::system_clock::now();
    auto end = start + boost::chrono::milliseconds(100);
    int64_t ms = TimeUtil::durationMs(start, end);
    EXPECT_GE(ms, 99);
    EXPECT_LE(ms, 150);
}

TEST(TimeUtil, ToMillisecondsString) {
    auto tp = boost::chrono::system_clock::now();
    std::string s = TimeUtil::toMillisecondsString(tp);
    EXPECT_FALSE(s.empty());
    for (char c : s) {
        EXPECT_TRUE(c >= '0' && c <= '9');
    }
}

TEST(TimeUtil, GetSystemTimezone) {
    std::string tz = TimeUtil::getSystemTimezone();
    EXPECT_FALSE(tz.empty());
    EXPECT_EQ(tz.size(), 6u);
    EXPECT_TRUE(tz[0] == '+' || tz[0] == '-');
}

TEST(TimeUtil, NowMsReturnsReasonableValue) {
    int64_t ms = TimeUtil::nowMs();
    /* 2024-01-01T00:00:00Z = 1704067200000 ms */
    EXPECT_GT(ms, 1704067200000LL);
    /* 应该在 2100 年之前 */
    EXPECT_LT(ms, 4102444800000LL);
}

TEST(TimeUtil, NowMsMonotonic) {
    int64_t a = TimeUtil::nowMs();
    int64_t b = TimeUtil::nowMs();
    EXPECT_GE(b, a);
}

} // namespace fw
} // namespace intertwine
