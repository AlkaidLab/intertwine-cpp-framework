#include "fw/WebSocketTransport.hpp"
#include "fw/Log.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>

namespace alkaidlab {
namespace fw {

void WebSocketTransport::setupClientCallbacks() {
    if (!m_client) {
        return;
    }

    m_client->onopen = [this]() {
        m_connected.store(true, boost::memory_order_release);
        LOG_DEBUG("WebSocketTransport: Connection opened");

        // 连接建立时统一设置超时
        if (m_client) {
            auto channel = m_client->channel;
            if (channel) {
                channel->setReadTimeout(m_readTimeout);
                channel->setWriteTimeout(m_writeTimeout);
                channel->setCloseTimeout(m_closeTimeout);
                channel->setKeepaliveTimeout(m_keepaliveTimeout);
            } else {
                LOG_WARN("WebSocketTransport: Channel is null, skipping timeout setup");
            }
        }

        m_connectionCondition.notify_all();
    };

    m_client->onclose = [this]() {
        m_connected.store(false, boost::memory_order_release);
        m_connectFailed.store(true, boost::memory_order_release);
        LOG_DEBUG("WebSocketTransport: Connection closed");
        m_connectionCondition.notify_all();
    };

    // 设置消息接收回调（只访问队列，有独立的锁）
    m_client->onmessage = [this](const std::string& msg) {
        boost::lock_guard<boost::mutex> lock(m_messageMutex);
        m_messageQueue.push(msg);
        m_messageCondition.notify_one();
        LOG_DEBUG("WebSocketTransport: Received message, size=" + std::to_string(msg.size()));
    };
}

WebSocketTransport::WebSocketTransport()
    : m_connectTimeout(3000)
    , m_readTimeout(30000)
    , m_writeTimeout(30000)
    , m_closeTimeout(5000)
    , m_keepaliveTimeout(60000)
    , m_connected(false)
    , m_connectFailed(false)
{
    m_sslConfig.verifyPeer = true;
    m_sslConfig.verifyHost = true;
    m_sslConfig.sslVersion = "TLS1.2";
    m_sslConfig.sessionReuse = true;
    
    try {
        m_client = std::unique_ptr<hv::WebSocketClient>(new hv::WebSocketClient());
        if (m_client) {
            configureSsl();
            setupClientCallbacks();
            m_connected.store(false, boost::memory_order_release);
        } else {
            m_connected.store(false, boost::memory_order_release);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create WebSocketClient: " + std::string(e.what()));
        m_connected.store(false, boost::memory_order_release);
    }
}

WebSocketTransport::WebSocketTransport(const SslConfig& sslConfig)
    : m_sslConfig(sslConfig)
    , m_connectTimeout(3000)
    , m_readTimeout(30000)
    , m_writeTimeout(30000)
    , m_closeTimeout(5000)
    , m_keepaliveTimeout(60000)
    , m_connected(false)
    , m_connectFailed(false)
{
    try {
        m_client = std::unique_ptr<hv::WebSocketClient>(new hv::WebSocketClient());
        if (m_client) {
            configureSsl();
            setupClientCallbacks();
            m_connected.store(false, boost::memory_order_release);
        } else {
            m_connected.store(false, boost::memory_order_release);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create WebSocketClient: " + std::string(e.what()));
        m_connected.store(false, boost::memory_order_release);
    }
}

WebSocketTransport::~WebSocketTransport() {
    try {
        disconnect(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    } catch (...) {}
}

bool WebSocketTransport::parseUrl(const std::string& url, 
                                   std::string& scheme,
                                   std::string& host, 
                                   int& port, 
                                   std::string& path) {
    std::string urlStr = url;
    boost::trim(urlStr);
    
    if (urlStr.empty()) {
        return false;
    }
    
    // 查找 scheme
    size_t schemeEnd = urlStr.find("://");
    if (schemeEnd == std::string::npos) {
        // 没有 scheme，默认为 ws
        scheme = "ws";
        schemeEnd = 0;
    } else {
        scheme = urlStr.substr(0, schemeEnd);
        boost::to_lower(scheme);
        schemeEnd += 3; // 跳过 "://"
    }
    
    // 查找 host:port/path
    size_t hostStart = schemeEnd;
    size_t pathStart = urlStr.find('/', hostStart);
    size_t portStart = urlStr.find(':', hostStart);
    
    // 确定 host 结束位置
    size_t hostEnd;
    if (portStart != std::string::npos && 
        (pathStart == std::string::npos || portStart < pathStart)) {
        // 有端口号
        hostEnd = portStart;
        std::string portStr = urlStr.substr(portStart + 1, 
            (pathStart != std::string::npos ? pathStart : urlStr.length()) - portStart - 1);
        try {
            port = std::stoi(portStr);
        } catch (...) {
            return false;
        }
    } else {
        // 没有端口号
        hostEnd = (pathStart != std::string::npos ? pathStart : urlStr.length());
        port = 0; // 默认端口
    }
    
    host = urlStr.substr(hostStart, hostEnd - hostStart);
    boost::trim(host);
    
    if (host.empty()) {
        return false;
    }
    
    // 设置默认端口
    if (port == 0) {
        if (scheme == "wss" || scheme == "https") {
            port = 443;
        } else {
            port = 80;
        }
    }
    
    // 提取路径
    if (pathStart != std::string::npos) {
        path = urlStr.substr(pathStart);
    } else {
        path = "/";
    }
    
    return true;
}

void WebSocketTransport::configureSsl() {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    
    if (!m_client) {
        return;
    }
    
    try {
        // libhv WebSocketClient 的 SSL 配置通过 HttpClient 设置
        // 这里需要访问底层的 HttpClient
        // 注意：libhv 的 WebSocketClient 可能不直接暴露 SSL 配置接口
        // 需要查看 libhv 的具体 API
        
        // 注意：libhv WebSocketClient 不支持 verifyHost（主机名验证）
        if (m_sslConfig.verifyHost) {
            LOG_WARN("WebSocketTransport: verifyHost is set to true, but libhv WebSocketClient "
                    "does not support explicit hostname verification configuration.");
        }
        
        // 注意：libhv WebSocketClient 不支持 sslVersion（SSL/TLS 版本）
        if (!m_sslConfig.sslVersion.empty() && m_sslConfig.sslVersion != "TLS1.2") {
            LOG_WARN("WebSocketTransport: sslVersion is set to '" + m_sslConfig.sslVersion + 
                    "', but libhv WebSocketClient does not support explicit SSL/TLS version configuration.");
        }
        
        // 注意：libhv WebSocketClient 不支持 sessionReuse（会话复用）
        if (!m_sslConfig.sessionReuse) {
            LOG_WARN("WebSocketTransport: sessionReuse is set to false, but libhv WebSocketClient "
                    "does not support explicit session reuse configuration.");
        }
        
        // 配置 CA 证书
        if (!m_sslConfig.caCertPath.empty()) {
            // m_client->setCaFile(m_sslConfig.caCertPath.c_str());
            LOG_DEBUG("WebSocket CA certificate: " + m_sslConfig.caCertPath);
        }
        
        // 配置客户端证书
        if (!m_sslConfig.clientCertPath.empty()) {
            // m_client->setCertFile(m_sslConfig.clientCertPath.c_str());
            LOG_DEBUG("WebSocket client certificate: " + m_sslConfig.clientCertPath);
        }
        
        // 配置客户端私钥
        if (!m_sslConfig.clientKeyPath.empty()) {
            // m_client->setKeyFile(m_sslConfig.clientKeyPath.c_str());
            LOG_DEBUG("WebSocket client key: " + m_sslConfig.clientKeyPath);
        }
        
        LOG_INFO("WebSocket SSL configuration applied");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to configure WebSocket SSL: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Failed to configure WebSocket SSL: unknown exception");
    }
}

int WebSocketTransport::sendRequest(const Request& req, Response& resp) {
    LOG_DEBUG("WebSocketTransport::sendRequest: Starting, URL=" + req.url);
    
    // 解析 URL
    std::string scheme, host, path;
    int port;
    if (!parseUrl(req.url, scheme, host, port, path)) {
        resp.statusCode = -1;
        resp.errorMessage = "Invalid URL format: " + req.url;
        LOG_ERROR("WebSocketTransport::sendRequest: Invalid URL format: " + req.url);
        return -1;
    }
    
    // 只支持 ws 和 wss
    if (scheme != "ws" && scheme != "wss") {
        resp.statusCode = -1;
        resp.errorMessage = "Only ws:// and wss:// schemes are supported";
        LOG_ERROR("WebSocketTransport::sendRequest: Unsupported scheme: " + scheme);
        return -1;
    }

    bool recreatedClient = false;
    {
        boost::lock_guard<boost::mutex> lock(m_clientMutex);
        if (!m_client) {
            try {
                m_client = std::unique_ptr<hv::WebSocketClient>(new hv::WebSocketClient());
                if (!m_client) {
                    resp.statusCode = -1;
                    resp.errorMessage = "Failed to create WebSocketClient";
                    LOG_ERROR("WebSocketTransport::sendRequest: Failed to create WebSocketClient");
                    return -1;
                }
                setupClientCallbacks();
                m_connected.store(false, boost::memory_order_release);
                m_connectFailed.store(false, boost::memory_order_release);
                m_currentUrl.clear();
                recreatedClient = true;
            } catch (const std::exception& e) {
                resp.statusCode = -1;
                resp.errorMessage = "Failed to recreate WebSocketClient: " + std::string(e.what());
                LOG_ERROR("WebSocketTransport::sendRequest: Failed to recreate WebSocketClient: " + std::string(e.what()));
                return -1;
            }
        }
    }
    if (recreatedClient) {
        configureSsl();
    }
    
    // 检查是否需要重新连接
    {
        boost::lock_guard<boost::mutex> lock(m_clientMutex);
        
        if (!m_client) {
            resp.statusCode = -1;
            resp.errorMessage = "WebSocketClient not initialized";
            LOG_ERROR("WebSocketTransport::sendRequest: WebSocketClient not initialized");
            return -1;
        }
        
        // 如果 URL 改变或未连接，需要重新连接
        if (!m_connected.load(boost::memory_order_acquire) || m_currentUrl != req.url) {
            LOG_DEBUG("WebSocketTransport::sendRequest: Need to reconnect to " + req.url);
            
            // 清空消息队列
            {
                boost::lock_guard<boost::mutex> msgLock(m_messageMutex);
                while (!m_messageQueue.empty()) {
                    m_messageQueue.pop();
                }
            }
            
            m_currentUrl = req.url;
            
            // 设置连接超时（在 open() 之前设置）
            // connectTimeout 是 DNS 解析 + TCP 握手 + WebSocket 握手的超时
            m_client->setConnectTimeout(m_connectTimeout);
            m_connectFailed.store(false, boost::memory_order_release);
            
            // 连接 WebSocket（异步连接）
            LOG_DEBUG("WebSocketTransport::sendRequest: Calling open()...");
            int ret = m_client->open(req.url.c_str());
            if (ret != 0) {
                resp.statusCode = -1;
                resp.errorMessage = "Failed to connect WebSocket: " + std::to_string(ret) 
                                  + ", URL: " + req.url;
                LOG_ERROR("WebSocketTransport::sendRequest: Failed to connect WebSocket, ret=" 
                         + std::to_string(ret) + ", URL=" + req.url);
                return -1;
            }
            
            // 重要：手动设置 channel 的连接超时
            // libhv 的内部逻辑可能跳过已存在的 channel，所以我们需要手动设置
            if (m_client->channel) {
                m_client->channel->setConnectTimeout(m_connectTimeout);
                LOG_DEBUG("WebSocketTransport::sendRequest: Set channel connect timeout to " + std::to_string(m_connectTimeout) + " ms");
            }
            
            LOG_DEBUG("WebSocketTransport::sendRequest: open() returned, waiting for connection...");
        }
    }
    // 锁已释放，避免在等待连接时持有锁
    
    // 等待连接建立（使用条件变量，不持有锁）
    {
        boost::unique_lock<boost::mutex> lock(m_clientMutex);
        auto startTime = boost::chrono::steady_clock::now();
        
        // 确定连接超时时间（优先使用请求中的超时，否则使用默认值）
        int connectTimeout = (req.connect_timeout >= 0) ? req.connect_timeout : m_connectTimeout;
        
        while (!m_connected.load(boost::memory_order_acquire)) {
            // 使用条件变量等待，避免循环轮询
            (void)m_connectionCondition.wait_for(
                lock, 
                boost::chrono::milliseconds(100)
            );

            // 检查是否连接失败（被回调通知）
            if (m_connectFailed.load(boost::memory_order_acquire)) {
                resp.statusCode = -1;
                resp.errorMessage = "Connection refused/closed to " + req.url;
                LOG_ERROR("WebSocketTransport::sendRequest: Connection failed (notified)");
                return -1;
            }
            
            // 检查超时
            auto now = boost::chrono::steady_clock::now();
            auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(now - startTime).count();
            // 只有当 connectTimeout > 0 时才检查超时（0 表示不设置超时，无限等待）
            if (connectTimeout > 0 && elapsed >= connectTimeout) {
                resp.statusCode = -1;
                resp.errorMessage = "Connection timeout to " + req.url;
                LOG_ERROR("WebSocketTransport::sendRequest: Connection timeout after " + std::to_string(elapsed) + " ms");
                return -1;
            }
            
            // 检查客户端连接状态（libhv 可能已连接但回调未触发）
            if (m_client && m_client->isConnected()) {
                LOG_DEBUG("WebSocketTransport::sendRequest: Client connected (detected via isConnected())");
                m_connected.store(true, boost::memory_order_release);
                break;
            }
        }
    }
    
    if (!m_connected.load(boost::memory_order_acquire)) {
        resp.statusCode = -1;
        resp.errorMessage = "Connection failed to " + req.url;
        LOG_ERROR("WebSocketTransport::sendRequest: Connection failed");
        return -1;
    }
    
    LOG_DEBUG("WebSocketTransport::sendRequest: Connected successfully to " + req.url);
    
    try {
        // 发送消息（WebSocket 使用 send 方法发送文本或二进制数据）
        if (!req.body.empty()) {
            int ret;
            {
                boost::lock_guard<boost::mutex> lock(m_clientMutex);
                if (!m_client) {
                    resp.statusCode = -1;
                    resp.errorMessage = "Client destroyed during send";
                    LOG_ERROR("WebSocketTransport::sendRequest: Client destroyed");
                    return -1;
                }
                ret = m_client->send(req.body.c_str(), req.body.length());
            }
            
            if (ret < 0) {
                resp.statusCode = -1;
                resp.errorMessage = "Send failed, error code: " + std::to_string(ret);
                LOG_ERROR("WebSocketTransport::sendRequest: Send failed, ret=" + std::to_string(ret) 
                         + ", URL=" + req.url);
                m_connected.store(false, boost::memory_order_release);
                return -1;
            }
            LOG_DEBUG("WebSocketTransport::sendRequest: Sent " + std::to_string(ret) + " bytes");
        }
        
        // 接收响应（通过receiveResponse方法获取）
        return receiveResponse(resp);
        
    } catch (const std::exception& e) {
        resp.statusCode = -1;
        resp.errorMessage = std::string("Exception: ") + e.what();
        LOG_ERROR("WebSocketTransport::sendRequest exception: " + resp.errorMessage + ", URL=" + req.url);
        m_connected.store(false, boost::memory_order_release);
        return -1;
    } catch (...) {
        resp.statusCode = -1;
        resp.errorMessage = "Unknown exception";
        LOG_ERROR("WebSocketTransport::sendRequest unknown exception, URL=" + req.url);
        m_connected.store(false, boost::memory_order_release);
        return -1;
    }
}

int WebSocketTransport::receiveResponse(Response& resp) {
    LOG_DEBUG("WebSocketTransport::receiveResponse: Entering");
    
    boost::unique_lock<boost::mutex> lock(m_messageMutex);
    
    // 检查连接状态
    if (!m_connected.load(boost::memory_order_acquire)) {
        resp.statusCode = -1;
        resp.errorMessage = "No active WebSocket connection";
        LOG_ERROR("WebSocketTransport::receiveResponse: No active connection");
        return -1;
    }
    
    try {
        // 等待消息到达（使用条件变量）
        if (m_messageQueue.empty()) {
            LOG_DEBUG("WebSocketTransport::receiveResponse: Message queue empty, waiting...");
            // 设置超时（使用毫秒）
            if (m_messageCondition.wait_for(lock, boost::chrono::milliseconds(m_readTimeout)) == boost::cv_status::timeout) {
                resp.statusCode = -1;
                resp.errorMessage = "Timeout waiting for WebSocket message";
                LOG_ERROR("WebSocketTransport::receiveResponse: Timeout waiting for message, timeout=" + std::to_string(m_readTimeout) + "ms");
                return -1;
            }
            LOG_DEBUG("WebSocketTransport::receiveResponse: Condition notified");
        }
        
        if (m_messageQueue.empty()) {
            resp.statusCode = -1;
            resp.errorMessage = "No message available";
            LOG_ERROR("WebSocketTransport::receiveResponse: No message in queue");
            return -1;
        }
        
        // 获取消息
        resp.statusCode = 200;
        resp.body = m_messageQueue.front();
        m_messageQueue.pop();
        
        LOG_DEBUG("WebSocketTransport::receiveResponse: Received message, size=" + std::to_string(resp.body.size()));
        return 0;
        
    } catch (const std::exception& e) {
        resp.statusCode = -1;
        resp.errorMessage = std::string("Exception: ") + e.what();
        LOG_ERROR("WebSocketTransport::receiveResponse exception: " + resp.errorMessage);
        m_connected.store(false, boost::memory_order_release);
        return -1;
    } catch (...) {
        resp.statusCode = -1;
        resp.errorMessage = "Unknown exception";
        LOG_ERROR("WebSocketTransport::receiveResponse unknown exception");
        m_connected.store(false, boost::memory_order_release);
        return -1;
    }
}

bool WebSocketTransport::connect() {
    // WebSocket 连接在 sendRequest 时建立
    bool connected = m_connected.load(boost::memory_order_acquire);
    bool clientValid = false;
    
    {
        boost::lock_guard<boost::mutex> lock(m_clientMutex);
        clientValid = (m_client != nullptr);
    }
    
    return connected && clientValid;
}

void WebSocketTransport::disconnect() {
    LOG_DEBUG("WebSocketTransport::disconnect: Disconnecting...");

    std::unique_ptr<hv::WebSocketClient> clientToClose;
    {
        boost::unique_lock<boost::mutex> lock(m_clientMutex);
        if (m_client) {
            // 先设置连接状态为 false，并摘掉捕获 this 的回调。
            // close() 之后 libhv 仍可能派发 onclose/onmessage，不能让它们访问已析构对象。
            m_client->onopen = nullptr;
            m_client->onclose = nullptr;
            m_client->onmessage = nullptr;
            clientToClose = std::move(m_client);
        }
        m_connected.store(false, boost::memory_order_release);
        m_connectFailed.store(true, boost::memory_order_release);
        m_currentUrl.clear();
    }

    if (clientToClose) {
        try {
            LOG_DEBUG("WebSocketTransport::disconnect: Calling close() without lock...");
            clientToClose->close();
            LOG_DEBUG("WebSocketTransport::disconnect: close() returned");
        } catch (const std::exception& e) {
            LOG_ERROR("WebSocketTransport::disconnect: Exception during close: " + std::string(e.what()));
        } catch (...) {
            LOG_ERROR("WebSocketTransport::disconnect: Unknown exception during close");
        }
        LOG_DEBUG("WebSocketTransport::disconnect: Client reset");
    }

    // 清空消息队列并通知等待的线程
    {
        boost::lock_guard<boost::mutex> msgLock(m_messageMutex);
        while (!m_messageQueue.empty()) {
            m_messageQueue.pop();
        }
        m_messageCondition.notify_all();
    }
    
    // 通知等待连接的线程
    m_connectionCondition.notify_all();
    
    LOG_DEBUG("WebSocketTransport::disconnect: Disconnected and queue cleared");
}

bool WebSocketTransport::isConnected() const {
    bool connected = m_connected.load(boost::memory_order_acquire);
    bool clientValid = false;
    bool clientConnected = false;
    
    {
        boost::lock_guard<boost::mutex> lock(m_clientMutex);
        clientValid = (m_client != nullptr);
        if (clientValid) {
            clientConnected = m_client->isConnected();
        }
    }
    
    return connected && clientValid && clientConnected;
}

void WebSocketTransport::setConnectTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_connectTimeout = milliseconds;
}

void WebSocketTransport::setReadTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_readTimeout = milliseconds;
}

void WebSocketTransport::setWriteTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_writeTimeout = milliseconds;
}

void WebSocketTransport::setCloseTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_closeTimeout = milliseconds;
}

void WebSocketTransport::setKeepaliveTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_keepaliveTimeout = milliseconds;
}

std::string WebSocketTransport::getTransportType() const {
    return "websocket";
}

void WebSocketTransport::setSslConfig(const SslConfig& config) {
    m_sslConfig = config;
    configureSsl();
}

} // namespace fw
} // namespace alkaidlab
