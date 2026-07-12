#include "intertwine/fw/Application.hpp"
#include "intertwine/fw/Context.hpp"
#include "intertwine/fw/Router.hpp"
#include <hv/HttpServer.h>
#include <hv/HttpService.h>
#include <hv/hssl.h>
#include <hv/hasync.h>
#include <cctype>
#include <cstring>
#include <set>
#include <sstream>

namespace intertwine {
namespace fw {

struct Application::Impl {
    hv::HttpService service;
    hv::HttpServer server;
    int httpsPort;
    std::set<std::string> allowedServerNames;

    Impl() : httpsPort(0) {}
};

Application::Application() : m_impl(new Impl) {}

Application::~Application() {
    delete m_impl;
}

void Application::setHeaderHandler(std::function<int(Context&)> fn) {
    m_impl->service.headerHandler = http_sync_handler(
        [fn](HttpRequest* req, HttpResponse* resp) -> int {
            Context c(static_cast<void*>(req), static_cast<void*>(resp));
            return fn(c);
        });
}

void Application::use(std::function<int(Context&)> fn) {
    m_impl->service.Use(
        [fn](HttpRequest* req, HttpResponse* resp) -> int {
            Context c(static_cast<void*>(req), static_cast<void*>(resp));
            return fn(c);
        });
}

void Application::setErrorHandler(std::function<int(Context&)> fn) {
    m_impl->service.errorHandler =
        [fn](HttpRequest* req, HttpResponse* resp) -> int {
            Context c(static_cast<void*>(req), static_cast<void*>(resp));
            return fn(c);
        };
}

void Application::setPostprocessor(std::function<int(Context&)> fn) {
    m_impl->service.postprocessor =
        [fn](HttpRequest* req, HttpResponse* resp) -> int {
            Context c(static_cast<void*>(req), static_cast<void*>(resp));
            return fn(c);
        };
}

void Application::setDocumentRoot(const std::string& path) {
    m_impl->service.document_root = path;
}

void Application::mountStatic(const std::string& urlPrefix, const std::string& dir) {
    m_impl->service.Static(urlPrefix.c_str(), dir.c_str());
}

void Application::mount(Router& router) {
    router.bind(m_impl->service);
}

void Application::setHost(const std::string& host) {
    strncpy(m_impl->server.host, host.c_str(), sizeof(m_impl->server.host) - 1);
    m_impl->server.host[sizeof(m_impl->server.host) - 1] = '\0';
}

void Application::setPort(int port) {
    m_impl->server.port = port;
}

void Application::setHttpsPort(int port) {
    m_impl->httpsPort = port;
}

void Application::setWorkerThreads(int n) {
    m_impl->server.setThreadNum(n);
}

void Application::setKeepaliveTimeout(int ms) {
    m_impl->service.keepalive_timeout = ms;
}

void Application::setLimitRate(int kbps) {
    m_impl->service.limit_rate = kbps;
}

void Application::proxy(const std::string& urlPrefix, const std::string& targetUrl) {
    m_impl->service.Proxy(urlPrefix.c_str(), targetUrl.c_str());
}

void Application::setProxyTimeout(int connectMs, int readMs, int writeMs) {
    m_impl->service.proxy_connect_timeout = connectMs;
    m_impl->service.proxy_read_timeout = readMs;
    m_impl->service.proxy_write_timeout = writeMs;
}

void Application::setServerName(const std::string& names) {
    m_impl->allowedServerNames.clear();
    if (names.empty()) return;
    std::istringstream iss(names);
    std::string token;
    while (std::getline(iss, token, ',')) {
        /* trim whitespace */
        std::string::size_type start = token.find_first_not_of(" \t");
        std::string::size_type end = token.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        token = token.substr(start, end - start + 1);
        /* lowercase */
        for (std::string::size_type i = 0; i < token.size(); ++i) {
            token[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(token[i])));
        }
        if (!token.empty()) {
            m_impl->allowedServerNames.insert(token);
        }
    }
}

const std::set<std::string>& Application::serverNames() const {
    return m_impl->allowedServerNames;
}

bool Application::isAllowedHost(const std::string& hostHeader) const {
    if (m_impl->allowedServerNames.empty()) return true;
    std::string host = hostHeader;
    /* strip port (e.g. "example.com:9339" → "example.com") */
    std::string::size_type colonPos = host.rfind(':');
    if (colonPos != std::string::npos) {
        /* ensure it's not part of IPv6 (check for ']') */
        if (host.find(']') == std::string::npos || colonPos > host.rfind(']')) {
            host = host.substr(0, colonPos);
        }
    }
    /* lowercase */
    for (std::string::size_type i = 0; i < host.size(); ++i) {
        host[i] = static_cast<char>(
            std::tolower(static_cast<unsigned char>(host[i])));
    }
    return m_impl->allowedServerNames.find(host)
           != m_impl->allowedServerNames.end();
}

bool Application::enableSsl(const std::string& certFile, const std::string& keyFile) {
    m_impl->server.port = 0;
    m_impl->server.https_port = m_impl->httpsPort > 0
        ? m_impl->httpsPort
        : m_impl->server.port;
    hssl_ctx_opt_t sslOpt;
    std::memset(&sslOpt, 0, sizeof(sslOpt));
    sslOpt.crt_file = certFile.c_str();
    sslOpt.key_file = keyFile.c_str();
    sslOpt.endpoint = 0; /* HSSL_SERVER */
    return m_impl->server.newSslCtx(&sslOpt) == 0;
}

int Application::start() {
    m_impl->server.service = &m_impl->service;
    return m_impl->server.start();
}

void Application::stop() {
    m_impl->server.stop();
}

int Application::connectionNum() const {
    return static_cast<int>(m_impl->server.connectionNum());
}

int Application::workerThreadsCount() const {
    return m_impl->server.worker_threads;
}

std::function<void(std::function<void()>)> Application::makeAsyncDispatcher() {
    return [](std::function<void()> task) {
        hv::async(task);
    };
}

void Application::cleanupAsync() {
    hv::async::cleanup();
}

} // namespace fw
} // namespace intertwine
