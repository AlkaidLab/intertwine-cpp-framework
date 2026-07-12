/**
 * HttpConstants.cpp — 编译期校验 fw::HttpStatus / fw::HttpMethod 与 libhv 枚举值一致。
 * 若上游 libhv 修改了枚举值，此处 static_assert 将立即触发编译错误。
 */
#include "intertwine/fw/HttpConstants.hpp"
#include <hv/httpdef.h>
#include <hv/HttpService.h>  // HTTP_STATUS_CLOSE macro

// ---- HttpStatus 校验 ----
#define FW_CHECK_STATUS(fw_name, hv_name) \
    static_assert(::intertwine::fw::HttpStatus::fw_name == (hv_name), \
                  "fw::HttpStatus::" #fw_name " mismatches libhv " #hv_name)

FW_CHECK_STATUS(Close,           HTTP_STATUS_CLOSE);
FW_CHECK_STATUS(Ok,              HTTP_STATUS_OK);
FW_CHECK_STATUS(NoContent,       HTTP_STATUS_NO_CONTENT);
FW_CHECK_STATUS(BadRequest,      HTTP_STATUS_BAD_REQUEST);
FW_CHECK_STATUS(Unauthorized,    HTTP_STATUS_UNAUTHORIZED);
FW_CHECK_STATUS(Forbidden,       HTTP_STATUS_FORBIDDEN);
FW_CHECK_STATUS(NotFound,        HTTP_STATUS_NOT_FOUND);
FW_CHECK_STATUS(TooManyRequests, HTTP_STATUS_TOO_MANY_REQUESTS);
FW_CHECK_STATUS(InternalError,   HTTP_STATUS_INTERNAL_SERVER_ERROR);

#undef FW_CHECK_STATUS

// ---- HttpMethod 校验 ----
#define FW_CHECK_METHOD(fw_name, hv_name) \
    static_assert(::intertwine::fw::HttpMethod::fw_name == (hv_name), \
                  "fw::HttpMethod::" #fw_name " mismatches libhv " #hv_name)

FW_CHECK_METHOD(DELETE_,  HTTP_DELETE);
FW_CHECK_METHOD(GET,      HTTP_GET);
FW_CHECK_METHOD(HEAD,     HTTP_HEAD);
FW_CHECK_METHOD(POST,     HTTP_POST);
FW_CHECK_METHOD(PUT,      HTTP_PUT);
FW_CHECK_METHOD(CONNECT,  HTTP_CONNECT);
FW_CHECK_METHOD(OPTIONS,  HTTP_OPTIONS);
FW_CHECK_METHOD(TRACE,    HTTP_TRACE);
FW_CHECK_METHOD(PATCH,    HTTP_PATCH);

#undef FW_CHECK_METHOD
