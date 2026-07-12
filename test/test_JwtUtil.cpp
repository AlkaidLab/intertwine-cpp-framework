#include "intertwine/fw/JwtUtil.hpp"
#include <gtest/gtest.h>
#include <boost/chrono.hpp>

namespace intertwine {
namespace fw {

static const std::string kSecret = "test_secret_key_for_jwt_testing_1234567890";

TEST(JwtUtil, SignAndVerify) {
    std::string token = JwtUtil::sign(kSecret, 3600);
    EXPECT_FALSE(token.empty());
    EXPECT_TRUE(JwtUtil::verify(token, kSecret));
}

TEST(JwtUtil, VerifyWrongSecret) {
    std::string token = JwtUtil::sign(kSecret, 3600);
    EXPECT_FALSE(JwtUtil::verify(token, "wrong_secret"));
}

TEST(JwtUtil, VerifyExpired) {
    std::string token = JwtUtil::sign(kSecret, -1);
    EXPECT_FALSE(JwtUtil::verify(token, kSecret));
}

TEST(JwtUtil, GetExpAndIat) {
    std::string token = JwtUtil::sign(kSecret, 7200);
    int64_t exp = JwtUtil::getExp(token);
    int64_t iat = JwtUtil::getIat(token);
    EXPECT_GT(exp, 0);
    EXPECT_GT(iat, 0);
    EXPECT_EQ(exp - iat, 7200);
}

TEST(JwtUtil, ExtractBearer) {
    EXPECT_EQ(JwtUtil::extractBearer("Bearer abc123"), "abc123");
    EXPECT_EQ(JwtUtil::extractBearer("bearer abc"), "");
    EXPECT_EQ(JwtUtil::extractBearer(""), "");
    EXPECT_EQ(JwtUtil::extractBearer("Basic abc"), "");
}

TEST(JwtUtil, RevokeAndIsRevoked) {
    std::string token = JwtUtil::sign(kSecret, 3600);
    EXPECT_FALSE(JwtUtil::isRevoked(token));
    JwtUtil::revoke(token);
    EXPECT_TRUE(JwtUtil::isRevoked(token));
    EXPECT_FALSE(JwtUtil::verify(token, kSecret));
}

/* ---- 重启踢出旧会话 ---- */

TEST(JwtUtil, StartTimeRejectsOldToken) {
    /* 确保先清除 startTime */
    JwtUtil::setStartTimeSeconds(0);

    std::string token = JwtUtil::sign(kSecret, 7200);
    EXPECT_TRUE(JwtUtil::verify(token, kSecret));

    int64_t iat = JwtUtil::getIat(token);
    ASSERT_GT(iat, 0);

    /* 模拟重启：设置启动时间为 iat + 1（未来 1 秒） */
    JwtUtil::setStartTimeSeconds(iat + 1);
    EXPECT_FALSE(JwtUtil::verify(token, kSecret));
}

TEST(JwtUtil, StartTimeAllowsNewToken) {
    int64_t now = static_cast<int64_t>(boost::chrono::duration_cast<boost::chrono::seconds>(
        boost::chrono::system_clock::now().time_since_epoch()).count());

    /* 设置启动时间为当前时间 */
    JwtUtil::setStartTimeSeconds(now);

    /* 签发 token（iat >= now） */
    std::string token = JwtUtil::sign(kSecret, 1800);
    int64_t iat = JwtUtil::getIat(token);
    EXPECT_GE(iat, now);
    EXPECT_TRUE(JwtUtil::verify(token, kSecret));
}

TEST(JwtUtil, StartTimeZeroDisablesCheck) {
    JwtUtil::setStartTimeSeconds(0);

    std::string token = JwtUtil::sign(kSecret, 900);
    EXPECT_TRUE(JwtUtil::verify(token, kSecret));
}

/* 清理：恢复默认状态 */
class JwtUtilCleanup : public ::testing::Environment {
public:
    void TearDown() BOOST_OVERRIDE {
        JwtUtil::setStartTimeSeconds(0);
        JwtUtil::cleanup();
    }
};

static ::testing::Environment* const kCleanup =
    ::testing::AddGlobalTestEnvironment(new JwtUtilCleanup);

} // namespace fw
} // namespace intertwine
