#include "intertwine/fw/HttpConstants.hpp"
#include <gtest/gtest.h>

namespace intertwine {
namespace fw {

// 验证 HttpStatus 值符合 HTTP 标准
TEST(HttpConstants, StatusValues) {
    EXPECT_EQ(HttpStatus::Next,           0);
    EXPECT_EQ(HttpStatus::Close,         -100);
    EXPECT_EQ(HttpStatus::Ok,            200);
    EXPECT_EQ(HttpStatus::NoContent,     204);
    EXPECT_EQ(HttpStatus::BadRequest,    400);
    EXPECT_EQ(HttpStatus::Unauthorized,  401);
    EXPECT_EQ(HttpStatus::Forbidden,     403);
    EXPECT_EQ(HttpStatus::NotFound,      404);
    EXPECT_EQ(HttpStatus::TooManyRequests, 429);
    EXPECT_EQ(HttpStatus::InternalError, 500);
}

// 验证 HttpMethod 枚举值
TEST(HttpConstants, MethodValues) {
    EXPECT_EQ(HttpMethod::DELETE_,  0);
    EXPECT_EQ(HttpMethod::GET,      1);
    EXPECT_EQ(HttpMethod::HEAD,     2);
    EXPECT_EQ(HttpMethod::POST,     3);
    EXPECT_EQ(HttpMethod::PUT,      4);
    EXPECT_EQ(HttpMethod::CONNECT,  5);
    EXPECT_EQ(HttpMethod::OPTIONS,  6);
    EXPECT_EQ(HttpMethod::TRACE,    7);
    EXPECT_EQ(HttpMethod::PATCH,   28);
}

// Close 应为负数（特殊内部状态，非 HTTP 标准码）
TEST(HttpConstants, CloseIsNegative) {
    EXPECT_LT(HttpStatus::Close, 0);
}

// Next（中间件放行）应为 0
TEST(HttpConstants, NextIsZero) {
    EXPECT_EQ(HttpStatus::Next, 0);
}

} // namespace fw
} // namespace intertwine
