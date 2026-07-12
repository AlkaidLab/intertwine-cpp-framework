#ifndef INTERTWINE_CPP_FRAMEWORK_TRANSPORT_TYPES_HPP
#define INTERTWINE_CPP_FRAMEWORK_TRANSPORT_TYPES_HPP

#include <string>
#include <memory>
#include <cstdint>

namespace intertwine {
namespace fw {

struct Request {
    std::string url;
    std::string method;
    std::string headers;
    std::string body;
    int timeout;
    int connect_timeout;
    int read_timeout;
    int write_timeout;
    int close_timeout;
    int keepalive_timeout;

    Request() : timeout(-1), connect_timeout(-1), read_timeout(-1), write_timeout(-1), close_timeout(-1), keepalive_timeout(-1) {}
};

struct Response {
    int statusCode;
    std::string headers;
    std::string body;
    std::string errorMessage;

    Response() : statusCode(0) {}
    bool isSuccess() const { return statusCode >= 200 && statusCode < 300; }
};

enum class ProtocolType { UNKNOWN = 0, MYSQL = 1, POSTGRESQL = 2 };

struct SslConfig {
    std::string caCertPath;
    std::string clientCertPath;
    std::string clientKeyPath;
    std::string clientKeyPassword;
    bool verifyPeer;
    bool verifyHost;
    std::string sslVersion;
    bool sessionReuse;

    SslConfig() : verifyPeer(true), verifyHost(true), sslVersion("TLS1.2"), sessionReuse(true) {}
};

} // namespace fw
} // namespace intertwine

#endif
