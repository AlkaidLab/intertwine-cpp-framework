#include "intertwine/fw/JsonUtil.hpp"
#include <gtest/gtest.h>

namespace intertwine {
namespace fw {

/* ── getString ─────────────────────────────────────── */

TEST(JsonUtilGetString, StringValue) {
    EXPECT_EQ(JsonUtil::getString(R"({"name":"alice"})", "name"), "alice");
}

TEST(JsonUtilGetString, IntegerAutoConvert) {
    EXPECT_EQ(JsonUtil::getString(R"({"port":8080})", "port"), "8080");
}

TEST(JsonUtilGetString, FloatAutoConvert) {
    std::string val = JsonUtil::getString(R"({"pi":3.14})", "pi");
    EXPECT_FALSE(val.empty());
    EXPECT_NEAR(std::stod(val), 3.14, 0.001);
}

TEST(JsonUtilGetString, MissingKeyReturnsDefault) {
    EXPECT_EQ(JsonUtil::getString(R"({"a":1})", "b", "fallback"), "fallback");
}

TEST(JsonUtilGetString, InvalidJsonReturnsDefault) {
    EXPECT_EQ(JsonUtil::getString("not json", "key", "def"), "def");
}

TEST(JsonUtilGetString, EmptyBodyReturnsDefault) {
    EXPECT_EQ(JsonUtil::getString("", "key"), "");
}

/* ── getInt ────────────────────────────────────────── */

TEST(JsonUtilGetInt, IntegerValue) {
    EXPECT_EQ(JsonUtil::getInt(R"({"count":42})", "count"), 42);
}

TEST(JsonUtilGetInt, MissingKeyReturnsDefault) {
    EXPECT_EQ(JsonUtil::getInt(R"({"a":1})", "b", -1), -1);
}

TEST(JsonUtilGetInt, StringValueReturnsDefault) {
    /* 字符串 "42" 不是 is_number_integer */
    EXPECT_EQ(JsonUtil::getInt(R"({"v":"42"})", "v", 0), 0);
}

TEST(JsonUtilGetInt, InvalidJsonReturnsDefault) {
    EXPECT_EQ(JsonUtil::getInt("{bad", "k", 99), 99);
}

/* ── getBool ───────────────────────────────────────── */

TEST(JsonUtilGetBool, TrueValue) {
    EXPECT_TRUE(JsonUtil::getBool(R"({"ok":true})", "ok"));
}

TEST(JsonUtilGetBool, FalseValue) {
    EXPECT_FALSE(JsonUtil::getBool(R"({"ok":false})", "ok", true));
}

TEST(JsonUtilGetBool, IntegerOneIsTrue) {
    EXPECT_TRUE(JsonUtil::getBool(R"({"v":1})", "v"));
}

TEST(JsonUtilGetBool, IntegerZeroIsFalse) {
    EXPECT_FALSE(JsonUtil::getBool(R"({"v":0})", "v", true));
}

TEST(JsonUtilGetBool, StringTrueIsTrue) {
    EXPECT_TRUE(JsonUtil::getBool(R"({"v":"true"})", "v"));
}

TEST(JsonUtilGetBool, StringOneIsTrue) {
    EXPECT_TRUE(JsonUtil::getBool(R"({"v":"1"})", "v"));
}

TEST(JsonUtilGetBool, MissingKeyReturnsDefault) {
    EXPECT_FALSE(JsonUtil::getBool(R"({"a":1})", "missing"));
    EXPECT_TRUE(JsonUtil::getBool(R"({"a":1})", "missing", true));
}

/* ── safeParse ─────────────────────────────────────── */

TEST(JsonUtilSafeParse, ValidJson) {
    auto j = JsonUtil::safeParse(R"({"x":1})");
    EXPECT_TRUE(j.contains("x"));
    EXPECT_EQ(j["x"], 1);
}

TEST(JsonUtilSafeParse, InvalidJsonReturnsEmptyObject) {
    auto j = JsonUtil::safeParse("not json");
    EXPECT_TRUE(j.is_object());
    EXPECT_TRUE(j.empty());
}

/* ── escape ────────────────────────────────────────── */

TEST(JsonUtilEscape, PlainString) {
    EXPECT_EQ(JsonUtil::escape("hello"), "hello");
}

TEST(JsonUtilEscape, QuotesEscaped) {
    EXPECT_EQ(JsonUtil::escape(R"(say "hi")"), R"(say \"hi\")");
}

TEST(JsonUtilEscape, BackslashEscaped) {
    EXPECT_EQ(JsonUtil::escape("a\\b"), "a\\\\b");
}

TEST(JsonUtilEscape, NewlineTabEscaped) {
    EXPECT_EQ(JsonUtil::escape("a\nb\tc"), "a\\nb\\tc");
}

TEST(JsonUtilEscape, CarriageReturnEscaped) {
    EXPECT_EQ(JsonUtil::escape("a\rb"), "a\\rb");
}

TEST(JsonUtilEscape, ControlCharsStripped) {
    std::string input = "a";
    input += '\x01';
    input += "b";
    EXPECT_EQ(JsonUtil::escape(input), "ab");
}

TEST(JsonUtilEscape, EmptyString) {
    EXPECT_EQ(JsonUtil::escape(""), "");
}

} // namespace fw
} // namespace intertwine
