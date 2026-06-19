#ifndef ALKAIDLAB_FW_WEBSOCKET_TRANSPORT_HPP
#define ALKAIDLAB_FW_WEBSOCKET_TRANSPORT_HPP

#include "fw/ITransport.hpp"
#include "fw/TransportTypes.hpp"

#include <hv/WebSocketClient.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/atomic.hpp>
#include <string>
#include <vector>
#include <queue>

namespace alkaidlab {
namespace fw {

class WebSocketTransport : public ITransport {
public:
    WebSocketTransport();
    explicit WebSocketTransport(const SslConfig& sslConfig);
    ~WebSocketTransport() override;

    int sendRequest(const Request& req, Response& resp) override;
    int receiveResponse(Response& resp) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string getTransportType() const override;

    void setSslConfig(const SslConfig& config);
    const SslConfig& getSslConfig() const { return m_sslConfig; }

    void setConnectTimeout(int milliseconds);
    void setReadTimeout(int milliseconds);
    void setWriteTimeout(int milliseconds);
    void setCloseTimeout(int milliseconds);
    void setKeepaliveTimeout(int milliseconds);

private:
    void configureSsl();
    void setupClientCallbacks();
    bool parseUrl(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path);

private:
    std::unique_ptr<hv::WebSocketClient> m_client;
    SslConfig m_sslConfig;
    int m_connectTimeout;
    int m_readTimeout;
    int m_writeTimeout;
    int m_closeTimeout;
    int m_keepaliveTimeout;

    boost::atomic<bool> m_connected;
    boost::atomic<bool> m_connectFailed;

    std::string m_currentUrl;

    mutable boost::mutex m_clientMutex;
    boost::condition_variable m_connectionCondition;

    std::queue<std::string> m_messageQueue;
    mutable boost::mutex m_messageMutex;
    boost::condition_variable m_messageCondition;
};

} // namespace fw
} // namespace alkaidlab

#endif
