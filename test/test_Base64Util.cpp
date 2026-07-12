#include "intertwine/fw/Base64.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace intertwine {
namespace fw {

TEST(Base64, EncodeDecodeString) {
    std::string raw = "hello";
    std::string enc = Base64::encode(raw);
    EXPECT_FALSE(enc.empty());
    std::string dec = Base64::decode(enc);
    EXPECT_EQ(dec, raw);
}

TEST(Base64, EncodeDecodeEmpty) {
    std::string raw;
    std::string enc = Base64::encode(raw);
    std::string dec = Base64::decode(enc);
    EXPECT_EQ(dec, raw);
}

TEST(Base64, EncodeDecodeBinary) {
    std::vector<unsigned char> data = {0x00, 0x01, 0xFF, 0x7F};
    std::string enc = Base64::encode(data);
    EXPECT_FALSE(enc.empty());
    std::string dec = Base64::decode(enc);
    EXPECT_EQ(dec.size(), 4u);
    EXPECT_EQ(static_cast<unsigned char>(dec[0]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(dec[1]), 0x01);
    EXPECT_EQ(static_cast<unsigned char>(dec[2]), 0xFF);
    EXPECT_EQ(static_cast<unsigned char>(dec[3]), 0x7F);
}

TEST(Base64, EncodeRawPointer) {
    unsigned char buf[] = "abc";
    std::string enc = Base64::encode(buf, 3);
    EXPECT_FALSE(enc.empty());
    EXPECT_EQ(Base64::decode(enc), "abc");
}

} // namespace fw
} // namespace intertwine
