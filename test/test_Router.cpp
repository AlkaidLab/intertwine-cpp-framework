#include "intertwine/fw/Router.hpp"
#include <gtest/gtest.h>
#include <hv/HttpMessage.h>
#include <hv/HttpService.h>

namespace intertwine {
namespace fw {

TEST(RouterTest, RouteCount) {
    Router router;
    EXPECT_EQ(router.routeCount(), 0u);
    router.get("/a", [](Context&) {});
    EXPECT_EQ(router.routeCount(), 1u);
    router.post("/b", [](Context&) {});
    router.put("/c", [](Context&) {});
    router.del("/d", [](Context&) {});
    router.patch("/e", [](Context&) {});
    EXPECT_EQ(router.routeCount(), 5u);
}

TEST(RouterTest, AsyncRoute) {
    Router router;
    router.getAsync("/download", [](Context&) {});
    EXPECT_EQ(router.routeCount(), 1u);
}

TEST(RouterTest, BindSyncRoutes) {
    Router router;
    bool called = false;
    router.get("/api/ping", [&called](Context& ctx) {
        called = true;
        ctx.json(200, "{\"ok\":true}");
    });
    hv::HttpService service;
    router.bind(service);

    // Invoke via libhv GetRoute
    HttpRequest req;
    req.method = HTTP_GET;
    req.path = "/api/ping";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(h != NULL);
    ASSERT_TRUE(h->sync_handler != NULL);
    h->sync_handler(&req, &resp);
    EXPECT_TRUE(called);
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.body, "{\"ok\":true}");
}

TEST(RouterTest, MiddlewarePlusHandler) {
    Router router;
    std::string order;
    router.use([&order](Context&, Next next) -> int {
        order += "M>";
        int r = next();
        order += "<M";
        return r;
    });
    router.get("/test", [&order](Context& ctx) {
        order += "H";
        ctx.setStatus(200);
    });
    hv::HttpService service;
    router.bind(service);

    HttpRequest req;
    req.method = HTTP_GET;
    req.path = "/test";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(h != NULL);
    h->sync_handler(&req, &resp);
    EXPECT_EQ(order, "M>H<M");
}

TEST(RouterTest, PostprocessorBinds) {
    Router router;
    bool ppCalled = false;
    router.setPostprocessor([&ppCalled](Context&) -> int {
        ppCalled = true;
        return 0;
    });
    hv::HttpService service;
    router.bind(service);
    EXPECT_TRUE(service.postprocessor.sync_handler != NULL);

    // Invoke postprocessor
    HttpRequest req;
    req.method = HTTP_GET;
    req.path = "/";
    HttpResponse resp;
    service.postprocessor.sync_handler(&req, &resp);
    EXPECT_TRUE(ppCalled);
}

TEST(RouterTest, ErrorHandlerBinds) {
    Router router;
    bool ehCalled = false;
    router.setErrorHandler([&ehCalled](Context& ctx) -> int {
        ehCalled = true;
        ctx.json(404, "{\"error\":\"not found\"}");
        return 404;
    });
    hv::HttpService service;
    router.bind(service);
    EXPECT_TRUE(service.errorHandler.sync_handler != NULL);

    HttpRequest req;
    req.method = HTTP_GET;
    req.path = "/not-exist";
    HttpResponse resp;
    int code = service.errorHandler.sync_handler(&req, &resp);
    EXPECT_TRUE(ehCalled);
    EXPECT_EQ(code, 404);
    EXPECT_EQ(resp.body, "{\"error\":\"not found\"}");
}

/* ---- Migration PoC tests ---- */

TEST(RouterTest, MigratedPingHandler) {
    Router router;
    router.get("/api/ping", [](Context& ctx) {
        ctx.json(200, "{\"service\":\"intertwine\",\"status\":\"ok\"}");
    });
    hv::HttpService service;
    router.bind(service);

    HttpRequest req;
    req.method = HTTP_GET;
    req.path = "/api/ping";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(h != NULL);
    h->sync_handler(&req, &resp);
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.body, "{\"service\":\"intertwine\",\"status\":\"ok\"}");
    EXPECT_EQ(resp.content_type, APPLICATION_JSON);
}

TEST(RouterTest, ExceptionGuardMiddleware) {
    Router router;
    router.use("exception_guard", [](Context& ctx, Next next) -> int {
        try {
            return next();
        } catch (const std::exception& e) {
            ctx.json(500, "{\"error\":\"internal_error\"}");
            return 500;
        } catch (...) {
            ctx.json(500, "{\"error\":\"unknown\"}");
            return 500;
        }
    });
    router.get("/api/throw", [](Context&) {
        throw std::runtime_error("test error");
    });
    hv::HttpService service;
    router.bind(service);

    HttpRequest req;
    req.method = HTTP_GET;
    req.path = "/api/throw";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(h != NULL);
    int code = h->sync_handler(&req, &resp);
    EXPECT_EQ(code, 500);
    EXPECT_EQ(resp.body, "{\"error\":\"internal_error\"}");
}

TEST(RouterTest, ExceptionGuardNoException) {
    Router router;
    router.use("exception_guard", [](Context& ctx, Next next) -> int {
        try {
            return next();
        } catch (...) {
            ctx.json(500, "{\"error\":\"unknown\"}");
            return 500;
        }
    });
    router.get("/api/ok", [](Context& ctx) {
        ctx.json(200, "{\"ok\":true}");
    });
    hv::HttpService service;
    router.bind(service);

    HttpRequest req;
    req.method = HTTP_GET;
    req.path = "/api/ok";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    h->sync_handler(&req, &resp);
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.body, "{\"ok\":true}");
}

TEST(RouterTest, BindPutRoute) {
    Router router;
    router.put("/api/config", [](Context& ctx) {
        ctx.json(200, "{\"updated\":true}");
    });
    hv::HttpService service;
    router.bind(service);

    HttpRequest req;
    req.method = HTTP_PUT;
    req.path = "/api/config";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(h != NULL);
    h->sync_handler(&req, &resp);
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.body, "{\"updated\":true}");
}

TEST(RouterTest, BindDeleteRoute) {
    Router router;
    router.del("/api/file", [](Context& ctx) {
        ctx.json(200, "{\"deleted\":true}");
    });
    hv::HttpService service;
    router.bind(service);

    HttpRequest req;
    req.method = HTTP_DELETE;
    req.path = "/api/file";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(h != NULL);
    h->sync_handler(&req, &resp);
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.body, "{\"deleted\":true}");
}

TEST(RouterTest, BindPatchRoute) {
    Router router;
    router.patch("/api/acl", [](Context& ctx) {
        ctx.json(200, "{\"patched\":true}");
    });
    hv::HttpService service;
    router.bind(service);

    HttpRequest req;
    req.method = HTTP_PATCH;
    req.path = "/api/acl";
    HttpResponse resp;
    http_handler* h = NULL;
    int rc = service.GetRoute(&req, &h);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(h != NULL);
    h->sync_handler(&req, &resp);
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.body, "{\"patched\":true}");
}

} // namespace fw
} // namespace intertwine
