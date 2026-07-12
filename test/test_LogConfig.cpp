#include "intertwine/fw/LogConfig.hpp"
#include <gtest/gtest.h>

namespace intertwine {
namespace fw {

TEST(LogConfig, LevelEnumValues) {
    EXPECT_EQ(LogConfig::kDebug, 0);
    EXPECT_EQ(LogConfig::kInfo,  1);
    EXPECT_EQ(LogConfig::kWarn,  2);
    EXPECT_EQ(LogConfig::kError, 3);
}

TEST(LogConfig, SetLevelDoesNotCrash) {
    // 静态函数调用不应崩溃
    LogConfig::setLevel(LogConfig::kInfo);
    LogConfig::setLevel(LogConfig::kDebug);
    LogConfig::setLevel(LogConfig::kError);
}

TEST(LogConfig, SetMaxFileSizeDoesNotCrash) {
    LogConfig::setMaxFileSize(16 * 1024 * 1024);
}

TEST(LogConfig, SetRemainDaysDoesNotCrash) {
    LogConfig::setRemainDays(7);
}

} // namespace fw
} // namespace intertwine
