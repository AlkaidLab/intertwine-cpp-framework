#ifndef INTERTWINE_CPP_FRAMEWORK_HV_TRANSPORT_HPP
#define INTERTWINE_CPP_FRAMEWORK_HV_TRANSPORT_HPP

/**
 * libhv-based HTTP/1.1 server transport implementation.
 *
 * Wraps hv::HttpServer + hv::HttpService, binding fw::Router to libhv's routing.
 * Provides the unified ServerTransport interface for upper layers.
 */

#include "intertwine/fw/ServerTransport.hpp"
#include <hv/HttpServer.h>
#include <hv/HttpService.h>
#include <string>
#include <cstring>

namespace intertwine {
namespace fw {

class HvServerTransport : public ServerTransport {
public:
    HvServerTransport();
    ~HvServerTransport() override;

    /* -- ServerTransport interface -- */

    int start() override;
    void stop() override;
    void setRouter(Router* router) override;
    int connectionCount() override;
    std::string type() const override;

    /* -- Configuration (call before start) -- */

    void setHost(const char* host);
    void setPort(int port);

    /** Enable HTTPS (single-port mode) */
    void setHttps(const char* certPath, const char* keyPath);

    /** Static file document root */
    void setDocumentRoot(const char* docRoot);

    /** Direct access to underlying hv::HttpServer (advanced config) */
    hv::HttpServer& server() { return m_server; }

    /** Direct access to underlying hv::HttpService */
    hv::HttpService& service() { return m_service; }

private:
    hv::HttpServer  m_server;
    hv::HttpService m_service;
    Router* m_router;
    int m_port;
    bool m_https;
    std::string m_certPath;
    std::string m_keyPath;
};

} // namespace fw
} // namespace intertwine

#endif // INTERTWINE_CPP_FRAMEWORK_HV_TRANSPORT_HPP
