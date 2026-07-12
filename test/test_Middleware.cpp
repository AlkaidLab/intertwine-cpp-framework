#include "intertwine/fw/Middleware.hpp"
#include "intertwine/fw/Context.hpp"
#include <gtest/gtest.h>
#include <string>

namespace intertwine {
namespace fw {

/* 辅助：通过 TestContextBuilder 构造一个最简 Context */
static Context makeCtx() {
    fw::TestContextBuilder b;
    b.method("GET").path("/test");
    return b.build();
}

TEST(MiddlewareTest, EmptyChainCallsHandler) {
    MiddlewareChain chain;
    Context ctx = makeCtx();
    bool called = false;
    int result = chain.execute(ctx, [&called](Context&) -> int {
        called = true;
        return 200;
    });
    EXPECT_TRUE(called);
    EXPECT_EQ(result, 200);
}

TEST(MiddlewareTest, SingleMiddleware) {
    MiddlewareChain chain;
    std::string order;
    chain.use([&order](Context&, Next next) -> int {
        order += "A>";
        int r = next();
        order += "<A";
        return r;
    });
    Context ctx = makeCtx();
    int result = chain.execute(ctx, [&order](Context&) -> int {
        order += "H";
        return 200;
    });
    EXPECT_EQ(order, "A>H<A");
    EXPECT_EQ(result, 200);
}

TEST(MiddlewareTest, OnionModelOrder) {
    MiddlewareChain chain;
    std::string order;
    chain.use("first", [&order](Context&, Next next) -> int {
        order += "1>";
        int r = next();
        order += "<1";
        return r;
    });
    chain.use("second", [&order](Context&, Next next) -> int {
        order += "2>";
        int r = next();
        order += "<2";
        return r;
    });
    chain.use("third", [&order](Context&, Next next) -> int {
        order += "3>";
        int r = next();
        order += "<3";
        return r;
    });
    Context ctx = makeCtx();
    int result = chain.execute(ctx, [&order](Context&) -> int {
        order += "H";
        return 200;
    });
    EXPECT_EQ(order, "1>2>3>H<3<2<1");
    EXPECT_EQ(result, 200);
}

TEST(MiddlewareTest, EarlyAbort) {
    MiddlewareChain chain;
    bool handlerCalled = false;
    chain.use([](Context& ctx, Next) -> int {
        ctx.setStatus(403);
        return 403;  // do NOT call next()
    });
    Context ctx = makeCtx();
    int result = chain.execute(ctx, [&handlerCalled](Context&) -> int {
        handlerCalled = true;
        return 200;
    });
    EXPECT_FALSE(handlerCalled);
    EXPECT_EQ(result, 403);
    EXPECT_EQ(ctx.status(), 403);
}

TEST(MiddlewareTest, MiddlewareModifiesContext) {
    MiddlewareChain chain;
    chain.use([](Context& ctx, Next next) -> int {
        ctx.set("user", "admin");
        return next();
    });
    Context ctx = makeCtx();
    std::string user;
    chain.execute(ctx, [&user](Context& c) -> int {
        user = c.get("user");
        return 200;
    });
    EXPECT_EQ(user, "admin");
}

TEST(MiddlewareTest, SizeAndClear) {
    MiddlewareChain chain;
    EXPECT_EQ(chain.size(), 0u);
    chain.use([](Context&, Next next) -> int { return next(); });
    chain.use("named", [](Context&, Next next) -> int { return next(); });
    EXPECT_EQ(chain.size(), 2u);
    chain.clear();
    EXPECT_EQ(chain.size(), 0u);
}

TEST(MiddlewareTest, AfterLogicRunsOnReturn) {
    MiddlewareChain chain;
    std::string headerVal;
    chain.use([&headerVal](Context& ctx, Next next) -> int {
        int r = next();
        // Set header after handler completes
        ctx.setHeader("X-After", "done");
        headerVal = ctx.responseHeader("X-After");
        return r;
    });
    Context ctx = makeCtx();
    chain.execute(ctx, [](Context& c) -> int {
        c.json(200, "{}");
        return 200;
    });
    EXPECT_EQ(headerVal, "done");
}

} // namespace fw
} // namespace intertwine
