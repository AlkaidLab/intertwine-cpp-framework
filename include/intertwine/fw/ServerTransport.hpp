#ifndef INTERTWINE_CPP_FRAMEWORK_SERVER_TRANSPORT_HPP
#define INTERTWINE_CPP_FRAMEWORK_SERVER_TRANSPORT_HPP

/**
 * Server-side transport interface.
 *
 * Decouples the network layer (HTTP/1.1, HTTP/2, QUIC, etc.)
 * from the handler/middleware/router layer.
 * Different protocols only need to implement this interface.
 */

#include <string>

namespace intertwine {
namespace fw {

class Router;

class ServerTransport {
public:
    virtual ~ServerTransport() {}

    /** Start listening. Returns 0 on success. */
    virtual int start() = 0;

    /** Graceful shutdown. */
    virtual void stop() = 0;

    /** Set the router (must be called before start()). */
    virtual void setRouter(Router* router) = 0;

    /** Number of active TCP connections. */
    virtual int connectionCount() = 0;

    /** Transport protocol type string (e.g. "http/1.1", "https/1.1"). */
    virtual std::string type() const = 0;
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_SERVER_TRANSPORT_HPP
