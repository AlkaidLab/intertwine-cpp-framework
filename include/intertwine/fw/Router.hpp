#ifndef INTERTWINE_CPP_FRAMEWORK_ROUTER_HPP
#define INTERTWINE_CPP_FRAMEWORK_ROUTER_HPP

/**
 * Framework router: unified handler signature + middleware chain + route table.
 *
 * Bridges to libhv's routing system via bind(), allowing old and new handlers
 * to coexist for gradual migration.
 * Header does NOT include any libhv headers (pimpl for bind target).
 */

#include "intertwine/fw/Context.hpp"
#include "intertwine/fw/Middleware.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hv { class HttpService; }

namespace intertwine {
namespace fw {

/** Unified handler signature: interacts only through Context& */
using Handler = std::function<void(Context&)>;

/** Receives raw request bytes without accumulating Context::body().
 * Destruction must discard unfinished work (disconnect or middleware rejection). */
class RequestBodyReceiver {
public:
    virtual ~RequestBodyReceiver() {}
    virtual void write(Context&, const char* data, size_t size) = 0;
    virtual void complete(Context&) = 0;
};
using RequestBodyFactory = std::function<std::shared_ptr<RequestBodyReceiver>(Context&)>;

class Router {
public:
    Router();

    /* -- Middleware registration -- */

    void use(MiddlewareFn fn);
    void use(const std::string& name, MiddlewareFn fn);

    /* -- Route registration -- */

    void get(const char* path, Handler handler);
    void getAsync(const char* path, Handler handler);
    void post(const char* path, Handler handler);
    void put(const char* path, Handler handler);
    /** Factory runs after headers; nullptr rejects the request using Context's error response. */
    void putStream(const char* path, RequestBodyFactory factory);
    void del(const char* path, Handler handler);
    void patch(const char* path, Handler handler);
    /** Register an HTTP method not used by the JSON API (for example WebDAV). */
    void custom(const char* method, const char* path, Handler handler);
    /** Async variant for streaming methods such as WebDAV GET. */
    void customAsync(const char* method, const char* path, Handler handler);

    /**
     * Set an async dispatcher.
     * asyncDispatcher submits the task to a thread pool.
     * If not set, async routes degrade to synchronous execution.
     */
    void setAsyncDispatcher(std::function<void(std::function<void()>)> dispatcher);

    /**
     * Set an async task tracker callback.
     * Called with +1 when an async task starts, -1 when it ends.
     * Used by the host application to track in-flight async tasks
     * (e.g. for graceful shutdown). Optional.
     */
    void setAsyncTaskTracker(std::function<void(int delta)> tracker);

    /** Set postprocessor (called after every request, for cache control/logging) */
    void setPostprocessor(std::function<int(Context&)> fn);

    /** Set error handler (called for unmatched routes) */
    void setErrorHandler(std::function<int(Context&)> fn);

    /**
     * Bind to hv::HttpService: bridges all routes and middleware to libhv.
     * After calling, the libhv HttpService is ready to serve.
     */
    void bind(hv::HttpService& service);

    size_t routeCount() const { return m_routes.size(); }

private:
    enum class Method { GET, POST, PUT, DEL, PATCH, CUSTOM };

    struct Route {
        Method method;
        std::string customMethod;
        std::string path;
        Handler handler;
        bool async;
        RequestBodyFactory bodyFactory;
    };

    MiddlewareChain m_middlewares;
    std::vector<Route> m_routes;
    std::function<int(Context&)> m_postprocessor;
    std::function<int(Context&)> m_errorHandler;
    std::function<void(std::function<void()>)> m_asyncDispatcher;
    std::function<void(int delta)> m_asyncTaskTracker;
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_ROUTER_HPP
