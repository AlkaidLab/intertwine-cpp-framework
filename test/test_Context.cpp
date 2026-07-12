#include "intertwine/fw/Context.hpp"
#include <gtest/gtest.h>
#include <hv/HttpMessage.h>
#include <memory>

namespace intertwine {
namespace fw {

// -- KV Store tests (no real HTTP needed) --

class ContextKV : public ::testing::Test {
protected:
    void SetUp() override {
        req.method = HTTP_GET;
        req.path = "/test";
    }
    HttpRequest  req;
    HttpResponse resp;
};

TEST_F(ContextKV, SetAndGet) {
    Context ctx(&req, &resp);
    ctx.set("foo", "bar");
    EXPECT_EQ(ctx.get("foo"), "bar");
}

TEST_F(ContextKV, GetDefault) {
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.get("missing"), "");
    EXPECT_EQ(ctx.get("missing", "default"), "default");
}

TEST_F(ContextKV, Has) {
    Context ctx(&req, &resp);
    EXPECT_FALSE(ctx.has("k"));
    ctx.set("k", "v");
    EXPECT_TRUE(ctx.has("k"));
}

TEST_F(ContextKV, OverwriteValue) {
    Context ctx(&req, &resp);
    ctx.set("k", "v1");
    ctx.set("k", "v2");
    EXPECT_EQ(ctx.get("k"), "v2");
}

TEST_F(ContextKV, EmptyValue) {
    Context ctx(&req, &resp);
    ctx.set("k", "");
    EXPECT_TRUE(ctx.has("k"));
    EXPECT_EQ(ctx.get("k"), "");
}

// -- Request access --

TEST_F(ContextKV, Method) {
    Context ctx(&req, &resp);
    EXPECT_STREQ(ctx.method(), "GET");
}

TEST_F(ContextKV, Path) {
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.path(), "/test");
}

TEST_F(ContextKV, Header) {
    req.SetHeader("X-Test", "hello");
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.header("X-Test"), "hello");
    EXPECT_EQ(ctx.header("X-Missing"), "");
}

TEST_F(ContextKV, Body) {
    req.body = "request body";
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.body(), "request body");
}

// -- Response --

TEST_F(ContextKV, Status) {
    Context ctx(&req, &resp);
    // libhv HttpResponse defaults to 200
    EXPECT_EQ(ctx.status(), 200);
    ctx.setStatus(404);
    EXPECT_EQ(ctx.status(), 404);
    ctx.setStatus(500);
    EXPECT_EQ(ctx.status(), 500);
}

TEST_F(ContextKV, SetHeader) {
    Context ctx(&req, &resp);
    ctx.setHeader("X-Out", "value");
    EXPECT_EQ(ctx.responseHeader("X-Out"), "value");
}

TEST_F(ContextKV, SetBody) {
    Context ctx(&req, &resp);
    ctx.setBody("hello");
    EXPECT_EQ(resp.body, "hello");
}

TEST_F(ContextKV, Json) {
    Context ctx(&req, &resp);
    ctx.json(200, "{\"ok\":true}");
    EXPECT_EQ(ctx.status(), 200);
    EXPECT_EQ(resp.body, "{\"ok\":true}");
    EXPECT_EQ(resp.content_type, APPLICATION_JSON);
}

// -- error() --

TEST_F(ContextKV, ErrorWithMessage) {
    Context ctx(&req, &resp);
    ctx.error(400, "bad_request", "invalid param");
    EXPECT_EQ(ctx.status(), 400);
    EXPECT_EQ(resp.body, "{\"error\":\"bad_request\",\"message\":\"invalid param\"}");
    EXPECT_EQ(resp.GetHeader("Content-Type"), "application/json; charset=utf-8");
}

TEST_F(ContextKV, ErrorWithoutMessage) {
    Context ctx(&req, &resp);
    ctx.error(404, "not_found");
    EXPECT_EQ(ctx.status(), 404);
    EXPECT_EQ(resp.body, "{\"error\":\"not_found\"}");
}

TEST_F(ContextKV, ErrorEscapesQuotes) {
    Context ctx(&req, &resp);
    ctx.error(500, "internal", "file \"test.txt\" not found");
    EXPECT_EQ(ctx.status(), 500);
    // message should have escaped quotes
    EXPECT_NE(resp.body.find("\\\"test.txt\\\""), std::string::npos);
}

TEST_F(ContextKV, ErrorNullMessage) {
    Context ctx(&req, &resp);
    ctx.error(403, "forbidden", nullptr);
    EXPECT_EQ(ctx.status(), 403);
    EXPECT_EQ(resp.body, "{\"error\":\"forbidden\"}");
}

TEST_F(ContextKV, ErrorEmptyMessage) {
    Context ctx(&req, &resp);
    ctx.error(401, "unauthorized", "");
    EXPECT_EQ(ctx.status(), 401);
    // empty message string should omit message field
    EXPECT_EQ(resp.body, "{\"error\":\"unauthorized\"}");
}

// -- param() --

TEST_F(ContextKV, Param) {
    req.SetParam("page", "2");
    req.SetParam("size", "20");
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.param("page"), "2");
    EXPECT_EQ(ctx.param("size"), "20");
    EXPECT_EQ(ctx.param("missing"), "");
}

// -- clientIp() / clientPort() --

TEST_F(ContextKV, ClientAddr) {
    req.client_addr.ip = "192.168.1.100";
    req.client_addr.port = 54321;
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.clientIp(), "192.168.1.100");
    EXPECT_EQ(ctx.clientPort(), 54321);
}

// -- setContentType() --

TEST_F(ContextKV, SetContentType) {
    Context ctx(&req, &resp);
    ctx.setContentType("text/plain");
    // libhv SetContentType sets the content_type enum field
    EXPECT_EQ(resp.content_type, TEXT_PLAIN);
}

// -- formData() --

TEST_F(ContextKV, FormData) {
    req.content_type = MULTIPART_FORM_DATA;
    req.SetFormData("cert", "CERT_CONTENT");
    req.SetFormData("key",  "KEY_CONTENT");
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.formData("cert"), "CERT_CONTENT");
    EXPECT_EQ(ctx.formData("key"),  "KEY_CONTENT");
    EXPECT_EQ(ctx.formData("missing"), "");
}

// -- cookie() / setCookie() --

TEST_F(ContextKV, CookieRead) {
    // libhv parses Cookie header during HTTP parse; in unit test populate directly
    HttpCookie ck1; ck1.name = "session"; ck1.value = "abc123";
    HttpCookie ck2; ck2.name = "lang";    ck2.value = "en";
    req.cookies.push_back(ck1);
    req.cookies.push_back(ck2);
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.cookie("session"), "abc123");
    EXPECT_EQ(ctx.cookie("lang"), "en");
}

TEST_F(ContextKV, CookieNotFound) {
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.cookie("missing"), "");
}

TEST_F(ContextKV, SetCookieLax) {
    Context ctx(&req, &resp);
    ctx.setCookie("token", "xyz", "/", 3600, true, 1);
    EXPECT_FALSE(resp.cookies.empty());
    bool found = false;
    for (size_t i = 0; i < resp.cookies.size(); ++i) {
        if (resp.cookies[i].name == "token") {
            EXPECT_EQ(resp.cookies[i].value, "xyz");
            EXPECT_EQ(resp.cookies[i].path, "/");
            EXPECT_EQ(resp.cookies[i].max_age, 3600);
            EXPECT_TRUE(resp.cookies[i].httponly);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ContextKV, SetCookieStrict) {
    Context ctx(&req, &resp);
    ctx.setCookie("id", "42", "/admin", 7200, true, 2);
    EXPECT_FALSE(resp.cookies.empty());
    bool found = false;
    for (size_t i = 0; i < resp.cookies.size(); ++i) {
        if (resp.cookies[i].name == "id") {
            EXPECT_EQ(resp.cookies[i].value, "42");
            EXPECT_EQ(resp.cookies[i].path, "/admin");
            EXPECT_EQ(resp.cookies[i].samesite, HttpCookie::Strict);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ContextKV, SetCookieSecure) {
    Context ctx(&req, &resp);
    ctx.setCookie("sid", "abc", "/", 60, true, 1, true);
    ASSERT_FALSE(resp.cookies.empty());
    bool found = false;
    for (size_t i = 0; i < resp.cookies.size(); ++i) {
        if (resp.cookies[i].name == "sid") {
            EXPECT_TRUE(resp.cookies[i].httponly);
            EXPECT_TRUE(resp.cookies[i].secure);
            EXPECT_EQ(resp.cookies[i].samesite, HttpCookie::Lax);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

// -- httpVersion() / methodEnum() --

TEST_F(ContextKV, HttpVersion) {
    req.http_major = 1;
    req.http_minor = 1;
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.httpVersion(), 11);
}

TEST_F(ContextKV, HttpVersionHttp2) {
    req.http_major = 2;
    req.http_minor = 0;
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.httpVersion(), 20);
}

TEST_F(ContextKV, MethodEnum) {
    req.method = HTTP_GET;
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.methodEnum(), static_cast<int>(HTTP_GET));
}

TEST_F(ContextKV, MethodEnumPost) {
    req.method = HTTP_POST;
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.methodEnum(), static_cast<int>(HTTP_POST));
}

// -- fullPath() --

TEST_F(ContextKV, FullPathNoQuery) {
    req.path = "/api/test";
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.fullPath(), "/api/test");
}

TEST_F(ContextKV, FullPathWithQueryParams) {
    req.path = "/api/list";
    req.SetParam("page", "1");
    req.SetParam("size", "20");
    Context ctx(&req, &resp);
    std::string fp = ctx.fullPath();
    EXPECT_NE(fp.find("page=1"), std::string::npos);
    EXPECT_NE(fp.find("size=20"), std::string::npos);
    EXPECT_EQ(fp.substr(0, 10), "/api/list?");
}

TEST_F(ContextKV, FullPathWithInlineQuery) {
    // If path already contains '?', don't append params again
    req.path = "/api/list?existing=yes";
    req.SetParam("extra", "val");
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.fullPath(), "/api/list?existing=yes");
}

// -- removeHeader() / eraseRequestHeader() --

TEST_F(ContextKV, RemoveResponseHeader) {
    Context ctx(&req, &resp);
    ctx.setHeader("X-Custom", "value");
    EXPECT_EQ(ctx.responseHeader("X-Custom"), "value");
    ctx.removeHeader("X-Custom");
    EXPECT_EQ(ctx.responseHeader("X-Custom"), "");
}

TEST_F(ContextKV, EraseRequestHeader) {
    req.SetHeader("X-Internal-User", "admin");
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.header("X-Internal-User"), "admin");
    ctx.eraseRequestHeader("X-Internal-User");
    EXPECT_EQ(ctx.header("X-Internal-User"), "");
}

// -- contentLength() / responseBodySize() --

TEST_F(ContextKV, ContentLength) {
    req.content_length = 1024;
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.contentLength(), 1024);
}

TEST_F(ContextKV, ContentLengthZero) {
    req.content_length = 0;
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.contentLength(), 0);
}

TEST_F(ContextKV, ResponseBodySize) {
    Context ctx(&req, &resp);
    EXPECT_EQ(ctx.responseBodySize(), 0u);
    ctx.setBody("hello world");
    EXPECT_EQ(ctx.responseBodySize(), 11u);
}

// -- setContentTypeByFilename() --

TEST_F(ContextKV, ContentTypeByFilename) {
    Context ctx(&req, &resp);
    ctx.setContentTypeByFilename("image.png");
    // libhv maps .png to image/png
    EXPECT_EQ(resp.content_type, IMAGE_PNG);
}

TEST_F(ContextKV, ContentTypeByFilenameJson) {
    Context ctx(&req, &resp);
    ctx.setContentTypeByFilename("data.json");
    EXPECT_EQ(resp.content_type, APPLICATION_JSON);
}

// -- writerOwnership --

TEST_F(ContextKV, WriterOwnershipNullForSyncHandler) {
    Context ctx(&req, &resp);
    std::shared_ptr<void> ownership = ctx.writerOwnership();
    EXPECT_EQ(ownership, nullptr);
}

TEST_F(ContextKV, WriterOwnershipNullForTestBuilder) {
    auto ctx = TestContextBuilder().method("GET").path("/test").build();
    EXPECT_EQ(ctx.writerOwnership(), nullptr);
}

} // namespace fw
} // namespace intertwine
