#include "intertwine/fw/TransportFactory.hpp"
#include "intertwine/fw/HttpTransport.hpp"
#include "intertwine/fw/HttpsTransport.hpp"
#include "intertwine/fw/TcpTransport.hpp"
#include "intertwine/fw/WebSocketTransport.hpp"

#include <boost/algorithm/string.hpp>

namespace intertwine {
namespace fw {

std::unique_ptr<ITransport> TransportFactory::create(const std::string& type) {
    // 使用Boost转换为小写
    std::string lowerType = boost::to_lower_copy(type);
    
    if (lowerType == "http") {
        return createHttp();
    }
    if (lowerType == "https") {
        return createHttps();
    }
    if (lowerType == "tcp") {
        return createTcp();
    }
    if (lowerType == "websocket" || lowerType == "ws") {
        return createWebSocket();
    }
    if (lowerType == "wss") {
        return createWebSocket(SslConfig());
    }
    
    return nullptr;
}

std::unique_ptr<ITransport> TransportFactory::createHttp() {
    return std::unique_ptr<ITransport>(new HttpTransport());
}

std::unique_ptr<ITransport> TransportFactory::createHttps(const SslConfig& sslConfig) {
    return std::unique_ptr<ITransport>(new HttpsTransport(sslConfig));
}

std::unique_ptr<ITransport> TransportFactory::createTcp() {
    return std::unique_ptr<ITransport>(new TcpTransport());
}

std::unique_ptr<ITransport> TransportFactory::createWebSocket(const SslConfig& sslConfig) {
    return std::unique_ptr<ITransport>(new WebSocketTransport(sslConfig));
}

} // namespace fw
} // namespace intertwine

