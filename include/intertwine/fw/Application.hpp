#ifndef INTERTWINE_CPP_FRAMEWORK_APPLICATION_HPP
#define INTERTWINE_CPP_FRAMEWORK_APPLICATION_HPP

/**
 * fw::Application — HTTP server lifecycle abstraction.
 *
 * Encapsulates libhv's HttpService + HttpServer behind a clean API:
 *   - Context-based callbacks (no raw request/response pointers in user code)
 *   - Service-level middleware, header handler, postprocessor
 *   - Static file serving, SSL, IO threads
 *   - Async task dispatch
 *
 * Usage:
 *   fw::Application app;
 *   app.setDocumentRoot("web");
 *   app.setHeaderHandler([](fw::Context& c) -> int { ... });
 *   app.use([](fw::Context& c) -> int { ... });
 *   app.setErrorHandler([](fw::Context& c) -> int { ... });
 *   fw::Router router;
 *   // ... register routes ...
 *   app.mount(router);
 *   app.setHost("0.0.0.0");
 *   app.setPort(9339);
 *   app.setWorkerThreads(4);
 *   app.start();
 *   // ... wait for shutdown ...
 *   app.stop();
 */

#include <functional>
#include <set>
#include <string>

namespace intertwine {
namespace fw {

class Context;
class Router;

class Application {
public:
    Application();
    ~Application();

    /* -- Service-level callbacks (Context-based) -- */

    /** Set header handler (called after headers parsed, before body received).
     *  Return 0 to continue, negative to close connection. */
    void setHeaderHandler(std::function<int(Context&)> fn);

    /** Add a service-level middleware (executed for ALL requests, before routing).
     *  Return 0 to continue, non-zero to short-circuit. */
    void use(std::function<int(Context&)> fn);

    /** Set error handler (called for unmatched routes / 404).
     *  Return HTTP status code or fw::HttpStatus::Close. */
    void setErrorHandler(std::function<int(Context&)> fn);

    /** Set postprocessor (called after every request for cleanup/logging).
     *  Return the response status code. */
    void setPostprocessor(std::function<int(Context&)> fn);

    /* -- Static file serving -- */

    void setDocumentRoot(const std::string& path);

    /** Mount a static directory at the given URL prefix.
     *  e.g. mountStatic("/downloads/", "data/uploads/public/")
     *  Equivalent to nginx: location /downloads/ { root data/uploads/public/; } */
    void mountStatic(const std::string& urlPrefix, const std::string& dir);

    /* -- Route mounting -- */

    /** Mount a Router's routes + route-level middleware into this application. */
    void mount(Router& router);

    /* -- Server configuration -- */

    void setHost(const std::string& host);
    void setPort(int port);
    void setHttpsPort(int port);
    void setWorkerThreads(int n);

    /** Set keepalive timeout in milliseconds. Default: 75000ms. */
    void setKeepaliveTimeout(int ms);

    /** Set send rate limit in KB/s. -1 = unlimited (default). */
    void setLimitRate(int kbps);

    /** Set reverse proxy: requests matching urlPrefix are forwarded to targetUrl.
     *  e.g. proxy("/api/v2/", "http://backend:8080/")
     *  Equivalent to nginx: location /api/v2/ { proxy_pass http://backend:8080/; } */
    void proxy(const std::string& urlPrefix, const std::string& targetUrl);

    /** Set proxy timeouts in milliseconds. */
    void setProxyTimeout(int connectMs, int readMs, int writeMs);

    /** Set allowed server names (comma-separated, case-insensitive).
     *  Empty string = accept all hosts. */
    void setServerName(const std::string& names);

    /** Get the current set of allowed server names (lowercase). */
    const std::set<std::string>& serverNames() const;

    /** Check if a Host header value matches the configured server_name list.
     *  Always returns true if server_name is not configured (empty). */
    bool isAllowedHost(const std::string& hostHeader) const;

    /** Enable HTTPS with cert/key PEM files. Returns false on failure. */
    bool enableSsl(const std::string& certFile, const std::string& keyFile);

    /* -- Lifecycle -- */

    /** Start the server (non-blocking). Returns 0 on success. */
    int start();

    /** Stop the server. */
    void stop();

    /* -- Monitoring -- */

    int connectionNum() const;
    int workerThreadsCount() const;

    /* -- Async support -- */

    /** Returns an async dispatcher that submits tasks to the global thread pool.
     *  Suitable for Router::setAsyncDispatcher(). */
    std::function<void(std::function<void()>)> makeAsyncDispatcher();

    /** Cleanup the global async thread pool (call during shutdown). */
    void cleanupAsync();

private:
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    struct Impl;
    Impl* m_impl;
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_APPLICATION_HPP
