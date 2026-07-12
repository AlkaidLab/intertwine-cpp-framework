#ifndef INTERTWINE_CPP_FRAMEWORK_HTTPS_TRANSPORT_HPP
#define INTERTWINE_CPP_FRAMEWORK_HTTPS_TRANSPORT_HPP

#include "intertwine/fw/ITransport.hpp"
#include "intertwine/fw/TransportTypes.hpp"

#include <hv/HttpClient.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <string>
#include <map>
#include <memory>

namespace intertwine {
namespace fw {

class HttpsTransport : public ITransport {
public:
    HttpsTransport();
    explicit HttpsTransport(const SslConfig& sslConfig);
    ~HttpsTransport() override;

    int sendRequest(const Request& req, Response& resp) override;
    int receiveResponse(Response& resp) override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string getTransportType() const override;

    void setSslConfig(const SslConfig& config);
    const SslConfig& getSslConfig() const { return m_sslConfig; }

    void setTimeout(int milliseconds);
    void setConnectTimeout(int milliseconds);
    void setReadTimeout(int milliseconds);
    void setWriteTimeout(int milliseconds);
    void setCloseTimeout(int milliseconds);
    void setKeepaliveTimeout(int milliseconds);

private:
    void configureSsl();
    bool parseUrl(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path);
    void parseHeaders(const std::string& headerStr, std::map<std::string, std::string>& headers);
    std::string buildHeadersString(const std::map<std::string, std::string>& headers);

    SslConfig m_sslConfig;
    std::unique_ptr<hv::HttpClient> m_client;
    int m_timeout;
    int m_connectTimeout;
    int m_readTimeout;
    int m_writeTimeout;
    int m_closeTimeout;
    int m_keepaliveTimeout;
    bool m_connected;
    boost::mutex m_clientMutex;
};

} // namespace fw
} // namespace intertwine

#endif
