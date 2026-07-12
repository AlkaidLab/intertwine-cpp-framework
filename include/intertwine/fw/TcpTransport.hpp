#ifndef INTERTWINE_CPP_FRAMEWORK_TCP_TRANSPORT_HPP
#define INTERTWINE_CPP_FRAMEWORK_TCP_TRANSPORT_HPP

#include "intertwine/fw/ITransport.hpp"
#include "intertwine/fw/TransportTypes.hpp"

#include <hv/TcpClient.h>
#include <hv/hloop.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/atomic.hpp>
#include <string>
#include <vector>
#include <queue>

namespace intertwine {
namespace fw {

class TcpTransport : public ITransport {
public:
    TcpTransport();
    ~TcpTransport() override;

    int sendRequest(const Request& req, Response& resp) override;
    int receiveResponse(Response& resp) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string getTransportType() const override;

    void setConnectTimeout(int milliseconds);
    void setReadTimeout(int milliseconds);
    void setWriteTimeout(int milliseconds);
    void setCloseTimeout(int milliseconds);
    void setKeepaliveTimeout(int milliseconds);

private:
    bool parseUrl(const std::string& url, std::string& host, int& port);
    void initUnpackSetting();
    void setupClient();
    int receiveResponseWithTimeout(Response& resp, int timeoutMilliseconds);
    void disconnectInternal();

private:
    std::unique_ptr<hv::TcpClient> m_client;
    int m_connectTimeout;
    int m_readTimeout;
    int m_writeTimeout;
    int m_closeTimeout;
    int m_keepaliveTimeout;

    boost::atomic<bool> m_connected;
    boost::atomic<bool> m_connectFailed;

    std::string m_currentHost;
    int m_currentPort;

    mutable boost::mutex m_clientMutex;
    boost::condition_variable m_connectionCondition;

    std::queue<std::vector<uint8_t>> m_packetQueue;
    mutable boost::mutex m_bufferMutex;
    boost::condition_variable m_packetCondition;

    unpack_setting_t m_unpackSetting;
};

} // namespace fw
} // namespace intertwine

#endif
