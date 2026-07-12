#include "intertwine/fw/IdUtil.hpp"
#include <gtest/gtest.h>
#include <string>
#include <set>

namespace intertwine {
namespace fw {

TEST(IdUtil, GenerateV4Format) {
    std::string u = IdUtil::generateV4();
    EXPECT_EQ(u.size(), 36u);
    EXPECT_EQ(u[8], '-');
    EXPECT_EQ(u[13], '-');
    EXPECT_EQ(u[18], '-');
    EXPECT_EQ(u[23], '-');
}

TEST(IdUtil, GenerateV4Unique) {
    std::set<std::string> seen;
    for (int i = 0; i < 20; ++i) {
        std::string u = IdUtil::generateV4();
        EXPECT_EQ(seen.count(u), 0u);
        seen.insert(u);
    }
}

TEST(SnowflakeGenerator, NextId) {
    SnowflakeGenerator g(0, 0);
    int64_t a = g.nextId();
    int64_t b = g.nextId();
    EXPECT_GT(a, 0);
    EXPECT_GE(b, a);
}

TEST(SnowflakeGenerator, NextIdStr) {
    SnowflakeGenerator g(1, 1);
    std::string s = g.nextIdStr();
    EXPECT_FALSE(s.empty());
    for (char c : s) {
        EXPECT_TRUE(c >= '0' && c <= '9');
    }
}

TEST(IdUtil, GenerateSnowflakeId) {
    IdUtil::initSnowflake(0, 0);
    int64_t id = IdUtil::generateSnowflakeId();
    EXPECT_GT(id, 0);
}

// --- isSafeId 测试 ---

TEST(IsSafeId, ValidUUID) {
    EXPECT_TRUE(isSafeId("550e8400-e29b-41d4-a716-446655440000"));
}

TEST(IsSafeId, AlphaNumeric) {
    EXPECT_TRUE(isSafeId("abc123"));
    EXPECT_TRUE(isSafeId("A"));
    EXPECT_TRUE(isSafeId("z"));
    EXPECT_TRUE(isSafeId("0"));
}

TEST(IsSafeId, EmptyString) {
    EXPECT_FALSE(isSafeId(""));
}

TEST(IsSafeId, TooLong) {
    std::string s(65, 'a');
    EXPECT_FALSE(isSafeId(s));
    std::string s64(64, 'a');
    EXPECT_TRUE(isSafeId(s64));
}

TEST(IsSafeId, DisallowedChars) {
    EXPECT_FALSE(isSafeId("../etc"));
    EXPECT_FALSE(isSafeId("foo/bar"));
    EXPECT_FALSE(isSafeId("foo bar"));
    EXPECT_FALSE(isSafeId("foo_bar"));
    EXPECT_FALSE(isSafeId("hello.txt"));
    EXPECT_FALSE(isSafeId(std::string("abc\0def", 7)));
}

TEST(IsSafeId, HyphenOnly) {
    EXPECT_TRUE(isSafeId("-"));
    EXPECT_TRUE(isSafeId("a-b-c"));
}

} // namespace fw
} // namespace intertwine
