#include "intertwine/fw/PasswordUtil.hpp"
#include <gtest/gtest.h>

namespace intertwine {
namespace fw {

TEST(PasswordUtil, HashProducesHashedFormat) {
    std::string hashed = PasswordUtil::hash("password123");
    EXPECT_TRUE(PasswordUtil::isHashed(hashed));
    EXPECT_EQ(hashed.substr(0, 15), "$pbkdf2-sha256$");
}

TEST(PasswordUtil, VerifyCorrectPassword) {
    std::string hashed = PasswordUtil::hash("mySecret");
    EXPECT_TRUE(PasswordUtil::verify("mySecret", hashed));
}

TEST(PasswordUtil, VerifyWrongPassword) {
    std::string hashed = PasswordUtil::hash("correct");
    EXPECT_FALSE(PasswordUtil::verify("wrong", hashed));
}

TEST(PasswordUtil, DifferentHashesForSamePassword) {
    /* 随机盐值，每次哈希不同 */
    std::string h1 = PasswordUtil::hash("same");
    std::string h2 = PasswordUtil::hash("same");
    EXPECT_NE(h1, h2);
    /* 但都能验证 */
    EXPECT_TRUE(PasswordUtil::verify("same", h1));
    EXPECT_TRUE(PasswordUtil::verify("same", h2));
}

TEST(PasswordUtil, IsHashedDetectsPlainText) {
    EXPECT_FALSE(PasswordUtil::isHashed("plaintext"));
    EXPECT_FALSE(PasswordUtil::isHashed(""));
    EXPECT_FALSE(PasswordUtil::isHashed("$2a$10$invalid_bcrypt"));
}

TEST(PasswordUtil, IsHashedDetectsValidHash) {
    EXPECT_TRUE(PasswordUtil::isHashed("$pbkdf2-sha256$310000$salt$hash"));
}

TEST(PasswordUtil, VerifyInvalidFormatReturnsFalse) {
    EXPECT_FALSE(PasswordUtil::verify("pw", "not_a_hash"));
    EXPECT_FALSE(PasswordUtil::verify("pw", ""));
}

TEST(PasswordUtil, EmptyPasswordHashable) {
    std::string hashed = PasswordUtil::hash("");
    EXPECT_TRUE(PasswordUtil::isHashed(hashed));
    EXPECT_TRUE(PasswordUtil::verify("", hashed));
    EXPECT_FALSE(PasswordUtil::verify("notempty", hashed));
}

} // namespace fw
} // namespace intertwine
