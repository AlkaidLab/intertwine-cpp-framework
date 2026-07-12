#include "intertwine/fw/HvTransport.hpp"
#include "intertwine/fw/Router.hpp"
#include <hv/hssl.h>

namespace intertwine {
namespace fw {

HvServerTransport::HvServerTransport()
    : m_router(nullptr)
    , m_port(8080)
    , m_https(false) {
    std::memset(m_server.host, 0, sizeof(m_server.host));
    std::strncpy(m_server.host, "0.0.0.0", sizeof(m_server.host) - 1);
}

HvServerTransport::~HvServerTransport() {
    stop();
}

/* -- Configuration -- */

void HvServerTransport::setHost(const char* host) {
    std::strncpy(m_server.host, host, sizeof(m_server.host) - 1);
    m_server.host[sizeof(m_server.host) - 1] = '\0';
}

void HvServerTransport::setPort(int port) {
    m_port = port;
}

void HvServerTransport::setHttps(const char* certPath, const char* keyPath) {
    m_https = true;
    m_certPath = certPath;
    m_keyPath = keyPath;
}

void HvServerTransport::setDocumentRoot(const char* docRoot) {
    m_service.document_root = docRoot;
}

/* -- ServerTransport interface -- */

void HvServerTransport::setRouter(Router* router) {
    m_router = router;
}

int HvServerTransport::start() {
    if (m_router) {
        m_router->bind(m_service);
    }
    m_server.service = &m_service;

    if (m_https) {
        m_server.port = 0;
        m_server.https_port = m_port;
        hssl_ctx_opt_t sslOpt;
        std::memset(&sslOpt, 0, sizeof(sslOpt));
        sslOpt.crt_file = m_certPath.c_str();
        sslOpt.key_file = m_keyPath.c_str();
        sslOpt.endpoint = 0; /* HSSL_SERVER */
        if (m_server.newSslCtx(&sslOpt) != 0) {
            return -1;
        }
    } else {
        m_server.port = m_port;
        m_server.https_port = 0;
    }

    return m_server.start();
}

void HvServerTransport::stop() {
    m_server.stop();
}

int HvServerTransport::connectionCount() {
    return m_server.connectionNum();
}

std::string HvServerTransport::type() const {
    return m_https ? "https/1.1" : "http/1.1";
}

} // namespace fw
} // namespace intertwine
