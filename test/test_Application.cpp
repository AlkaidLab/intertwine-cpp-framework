#include "intertwine/fw/Application.hpp"
#include "intertwine/fw/Context.hpp"
#include "intertwine/fw/HttpConstants.hpp"
#include "intertwine/fw/Router.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>

namespace intertwine {
namespace fw {

/* ── 基础构造/配置 ── */

TEST(Application, ConstructDestruct) {
    Application app;
    // 构造和析构不应崩溃
}

TEST(Application, SetDocumentRoot) {
    Application app;
    app.setDocumentRoot("/tmp");
}

TEST(Application, SetHostAndPort) {
    Application app;
    app.setHost("127.0.0.1");
    app.setPort(19339);
}

TEST(Application, SetWorkerThreads) {
    Application app;
    app.setWorkerThreads(2);
    EXPECT_EQ(app.workerThreadsCount(), 2);
}

TEST(Application, WorkerThreadsDefaultZero) {
    Application app;
    // libhv 默认 worker_threads = 0（由系统决定）
    EXPECT_EQ(app.workerThreadsCount(), 0);
}

TEST(Application, SetWorkerThreadsMultipleTimes) {
    Application app;
    app.setWorkerThreads(4);
    EXPECT_EQ(app.workerThreadsCount(), 4);
    app.setWorkerThreads(8);
    EXPECT_EQ(app.workerThreadsCount(), 8);
}

/* ── 回调注册 ── */

TEST(Application, UseMiddleware) {
    Application app;
    app.use([](Context& c) -> int {
        c.setHeader("X-Test", "ok");
        return HttpStatus::Next;
    });
}

TEST(Application, SetErrorHandler) {
    Application app;
    app.setErrorHandler([](Context& c) -> int {
        c.json(404, "{\"error\":\"not_found\"}");
        return HttpStatus::NotFound;
    });
}

TEST(Application, SetPostprocessor) {
    Application app;
    app.setPostprocessor([](Context& c) -> int {
        return c.status();
    });
}

TEST(Application, SetHeaderHandler) {
    Application app;
    app.setHeaderHandler([](Context& c) -> int {
        return 0; // continue
    });
}

/* ── 路由挂载 ── */

TEST(Application, MountRouter) {
    Application app;
    Router router;
    router.get("/test", [](Context& c) -> int {
        c.json(200, "{\"ok\":true}");
        return 200;
    });
    app.mount(router);
}

TEST(Application, MountEmptyRouter) {
    Application app;
    Router router;
    app.mount(router);
    // 空路由挂载不应崩溃
}

TEST(Application, MountMultipleRouters) {
    Application app;
    Router r1, r2;
    r1.get("/api/a", [](Context& c) { c.json(200, "{}"); });
    r2.post("/api/b", [](Context& c) { c.json(201, "{}"); });
    app.mount(r1);
    app.mount(r2);
}

/* ── 异步调度器 ── */

TEST(Application, MakeAsyncDispatcher) {
    Application app;
    auto dispatcher = app.makeAsyncDispatcher();
    EXPECT_TRUE(static_cast<bool>(dispatcher));
}

TEST(Application, AsyncDispatcherRunsTask) {
    Application app;
    auto dispatcher = app.makeAsyncDispatcher();
    std::atomic<bool> ran(false);
    dispatcher([&ran]() { ran.store(true); });
    // 等待异步任务完成
    for (int i = 0; i < 50 && !ran.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(ran.load());
    app.cleanupAsync();
}

/* ── 监控 ── */

TEST(Application, ConnectionNumBeforeStart) {
    Application app;
    EXPECT_EQ(app.connectionNum(), 0);
}

/* ── 生命周期 ── */

TEST(Application, StartStopLifecycle) {
    Application app;
    app.setHost("127.0.0.1");
    app.setPort(39339);
    app.setWorkerThreads(1);

    Router router;
    router.get("/health", [](Context& c) -> int {
        c.json(200, "{\"ok\":true}");
        return 200;
    });
    app.mount(router);

    EXPECT_EQ(app.start(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(app.workerThreadsCount(), 1);
    // connectionNum >= 0（服务器刚启动，无外部连接）
    EXPECT_GE(app.connectionNum(), 0);

    app.stop();
}

TEST(Application, StartStopWithMiddleware) {
    Application app;
    app.setHost("127.0.0.1");
    app.setPort(39340);
    app.setWorkerThreads(1);

    app.use([](Context& c) -> int {
        c.setHeader("X-Powered-By", "fw");
        return HttpStatus::Next;
    });
    app.setPostprocessor([](Context& c) -> int {
        return c.status();
    });

    Router router;
    router.get("/ping", [](Context& c) -> int {
        c.json(200, "{\"pong\":true}");
        return 200;
    });
    app.mount(router);

    EXPECT_EQ(app.start(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    app.stop();
}

/* ── 完整配置场景 ── */

TEST(Application, FullConfigurationScenario) {
    Application app;
    app.setHost("127.0.0.1");
    app.setPort(39341);
    app.setWorkerThreads(2);
    app.setDocumentRoot("/tmp");

    app.setHeaderHandler([](Context& c) -> int {
        return 0;
    });
    app.use([](Context& c) -> int {
        return HttpStatus::Next;
    });
    app.setErrorHandler([](Context& c) -> int {
        c.json(404, "{\"error\":\"not_found\"}");
        return HttpStatus::NotFound;
    });
    app.setPostprocessor([](Context& c) -> int {
        return c.status();
    });

    Router router;
    router.get("/api/test", [](Context& c) -> int {
        c.json(200, "{\"ok\":true}");
        return 200;
    });
    router.post("/api/data", [](Context& c) -> int {
        c.json(201, "{\"created\":true}");
        return 201;
    });
    app.mount(router);

    EXPECT_EQ(app.workerThreadsCount(), 2);
}

/* ── S1: mountStatic ── */

TEST(Application, MountStatic) {
    Application app;
    app.mountStatic("/downloads/", "/tmp/downloads");
    // 不崩溃即可
}

TEST(Application, MountStaticMultiple) {
    Application app;
    app.mountStatic("/assets/", "/var/www/assets");
    app.mountStatic("/media/", "/var/www/media");
}

/* ── S2: proxy ── */

TEST(Application, Proxy) {
    Application app;
    app.proxy("/api/v2/", "http://backend:8080/");
}

TEST(Application, ProxyMultiple) {
    Application app;
    app.proxy("/api/", "http://127.0.0.1:8080/");
    app.proxy("/legacy/", "http://old-server:3000/");
}

TEST(Application, SetProxyTimeout) {
    Application app;
    app.setProxyTimeout(3000, 60000, 30000);
}

/* ── S3: keepalive / limitRate ── */

TEST(Application, SetKeepaliveTimeout) {
    Application app;
    app.setKeepaliveTimeout(30000);
}

TEST(Application, SetLimitRate) {
    Application app;
    app.setLimitRate(1024); // 1MB/s
}

TEST(Application, SetLimitRateUnlimited) {
    Application app;
    app.setLimitRate(-1);
}

/* ── S4: serverName ── */

TEST(Application, ServerNameEmpty) {
    Application app;
    app.setServerName("");
    EXPECT_TRUE(app.serverNames().empty());
    EXPECT_TRUE(app.isAllowedHost("anything.com"));
}

TEST(Application, ServerNameSingle) {
    Application app;
    app.setServerName("example.com");
    EXPECT_EQ(app.serverNames().size(), 1u);
    EXPECT_TRUE(app.isAllowedHost("example.com"));
    EXPECT_TRUE(app.isAllowedHost("Example.COM"));
    EXPECT_TRUE(app.isAllowedHost("example.com:9339"));
    EXPECT_FALSE(app.isAllowedHost("evil.com"));
}

TEST(Application, ServerNameMultiple) {
    Application app;
    app.setServerName("a.com, b.com, c.com");
    EXPECT_EQ(app.serverNames().size(), 3u);
    EXPECT_TRUE(app.isAllowedHost("a.com"));
    EXPECT_TRUE(app.isAllowedHost("b.com"));
    EXPECT_TRUE(app.isAllowedHost("c.com"));
    EXPECT_FALSE(app.isAllowedHost("d.com"));
}

TEST(Application, ServerNameCaseInsensitive) {
    Application app;
    app.setServerName("Files.Example.COM");
    EXPECT_TRUE(app.isAllowedHost("files.example.com"));
    EXPECT_TRUE(app.isAllowedHost("FILES.EXAMPLE.COM"));
}

TEST(Application, ServerNameStripPort) {
    Application app;
    app.setServerName("myhost.lan");
    EXPECT_TRUE(app.isAllowedHost("myhost.lan:443"));
    EXPECT_TRUE(app.isAllowedHost("myhost.lan:9339"));
    EXPECT_FALSE(app.isAllowedHost("other.lan:9339"));
}

TEST(Application, ServerNameTrimWhitespace) {
    Application app;
    app.setServerName("  a.com , b.com  ,  c.com  ");
    EXPECT_EQ(app.serverNames().size(), 3u);
    EXPECT_TRUE(app.isAllowedHost("a.com"));
    EXPECT_TRUE(app.isAllowedHost("b.com"));
    EXPECT_TRUE(app.isAllowedHost("c.com"));
}

TEST(Application, ServerNameOverwrite) {
    Application app;
    app.setServerName("old.com");
    EXPECT_TRUE(app.isAllowedHost("old.com"));
    app.setServerName("new.com");
    EXPECT_FALSE(app.isAllowedHost("old.com"));
    EXPECT_TRUE(app.isAllowedHost("new.com"));
}

TEST(Application, ServerNameClearByEmpty) {
    Application app;
    app.setServerName("example.com");
    EXPECT_FALSE(app.isAllowedHost("other.com"));
    app.setServerName("");
    EXPECT_TRUE(app.isAllowedHost("other.com")); // 空 = 接受所有
}

TEST(Application, ServerNameSkipEmptyTokens) {
    Application app;
    app.setServerName(",, a.com ,,, b.com ,,");
    EXPECT_EQ(app.serverNames().size(), 2u);
    EXPECT_TRUE(app.isAllowedHost("a.com"));
    EXPECT_TRUE(app.isAllowedHost("b.com"));
}

} // namespace fw
} // namespace intertwine
