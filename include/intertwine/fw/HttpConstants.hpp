#ifndef INTERTWINE_CPP_FRAMEWORK_HTTP_CONSTANTS_HPP
#define INTERTWINE_CPP_FRAMEWORK_HTTP_CONSTANTS_HPP

namespace intertwine {
namespace fw {

/** HTTP 状态码常量（与 libhv 数值一致，header-only） */
namespace HttpStatus {
    /** 继续处理（中间件放行） */
    static const int Next = 0;
    /** 关闭 TCP 连接，不发送任何 HTTP 响应 */
    static const int Close = -100;
    static const int Ok = 200;
    static const int NoContent = 204;
    static const int PartialContent = 206;
    static const int BadRequest = 400;
    static const int Unauthorized = 401;
    static const int Forbidden = 403;
    static const int NotFound = 404;
    static const int PayloadTooLarge = 413;
    static const int TooManyRequests = 429;
    static const int InternalError = 500;
}

/** HTTP 方法枚举值（与 libhv http_method 数值一致） */
namespace HttpMethod {
    static const int DELETE_ = 0;
    static const int GET = 1;
    static const int HEAD = 2;
    static const int POST = 3;
    static const int PUT = 4;
    static const int CONNECT = 5;
    static const int OPTIONS = 6;
    static const int TRACE = 7;
    static const int PATCH = 28;
}

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_HTTP_CONSTANTS_HPP
