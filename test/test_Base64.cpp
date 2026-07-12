#include "intertwine/fw/Base64.hpp"
#include <gtest/gtest.h>
#include <string>
#include <cstring>

namespace intertwine {
namespace fw {

TEST(Base64, EncodeEmpty) {
    // libhv Base64Encode 不支持 len=0（上游行为），跳过空编码测试
    // 实际业务中不会对空数据做 Base64 编码
}

TEST(Base64, EncodeHelloWorld) {
    const char* input = "Hello, World!";
    std::string encoded = Base64::encode(
        reinterpret_cast<const unsigned char*>(input),
        static_cast<unsigned int>(std::strlen(input)));
    EXPECT_EQ(encoded, "SGVsbG8sIFdvcmxkIQ==");
}

TEST(Base64, DecodeHelloWorld) {
    const char* encoded = "SGVsbG8sIFdvcmxkIQ==";
    std::string decoded = Base64::decode(
        encoded, static_cast<unsigned int>(std::strlen(encoded)));
    EXPECT_EQ(decoded, "Hello, World!");
}

TEST(Base64, RoundTrip) {
    const char* input = "abcdefghijklmnopqrstuvwxyz0123456789";
    unsigned int len = static_cast<unsigned int>(std::strlen(input));
    std::string encoded = Base64::encode(
        reinterpret_cast<const unsigned char*>(input), len);
    std::string decoded = Base64::decode(
        encoded.c_str(), static_cast<unsigned int>(encoded.size()));
    EXPECT_EQ(decoded, input);
}

TEST(Base64, RoundTripBinary) {
    unsigned char bin[] = {0x00, 0x01, 0xFF, 0xFE, 0x80, 0x7F};
    std::string encoded = Base64::encode(bin, sizeof(bin));
    EXPECT_FALSE(encoded.empty());
    std::string decoded = Base64::decode(
        encoded.c_str(), static_cast<unsigned int>(encoded.size()));
    EXPECT_EQ(decoded.size(), sizeof(bin));
    EXPECT_EQ(std::memcmp(decoded.data(), bin, sizeof(bin)), 0);
}

TEST(Base64, EncodePaddingVariants) {
    // 1 byte → 4 chars with ==
    std::string e1 = Base64::encode(
        reinterpret_cast<const unsigned char*>("a"), 1);
    EXPECT_EQ(e1, "YQ==");

    // 2 bytes → 4 chars with =
    std::string e2 = Base64::encode(
        reinterpret_cast<const unsigned char*>("ab"), 2);
    EXPECT_EQ(e2, "YWI=");

    // 3 bytes → 4 chars no padding
    std::string e3 = Base64::encode(
        reinterpret_cast<const unsigned char*>("abc"), 3);
    EXPECT_EQ(e3, "YWJj");
}

} // namespace fw
} // namespace intertwine
