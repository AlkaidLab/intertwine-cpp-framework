#include "intertwine/fw/TcpTransport.hpp"
#include "intertwine/fw/Log.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace intertwine {
namespace fw {

TcpTransport::TcpTransport()
    : m_connectTimeout(3000)
    , m_readTimeout(30000)
    , m_writeTimeout(30000)
    , m_closeTimeout(5000)
    , m_keepaliveTimeout(60000)
    , m_connected(false)
    , m_connectFailed(false)
    , m_currentPort(0)
{
    try {
        m_client = std::unique_ptr<hv::TcpClient>(new hv::TcpClient());
        if (m_client) {
            setupClient();
            m_connected.store(false, boost::memory_order_release);
        } else {
            m_connected.store(false, boost::memory_order_release);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create TcpClient: " + std::string(e.what()));
        m_connected.store(false, boost::memory_order_release);
    }
}

TcpTransport::~TcpTransport() {
    try {
        disconnect(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    } catch (...) {}
}

bool TcpTransport::parseUrl(const std::string& url, std::string& host, int& port) {
    std::string urlStr = url;
    boost::trim(urlStr);
    
    if (urlStr.empty()) {
        return false;
    }
    
    // 移除 scheme（如果有）
    size_t schemeEnd = urlStr.find("://");
    size_t hostStart = 0;
    if (schemeEnd != std::string::npos) {
        hostStart = schemeEnd + 3;
    }
    
    // 查找 host:port
    size_t portStart = urlStr.find(':', hostStart);
    size_t pathStart = urlStr.find('/', hostStart);
    
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
    
    // 设置默认端口（TCP 通常使用 80 或自定义端口）
    if (port == 0) {
        port = 80; // 默认端口
    }
    
    return true;
}

void TcpTransport::initUnpackSetting() {
    memset(&m_unpackSetting, 0, sizeof(m_unpackSetting));
    m_unpackSetting.mode = UNPACK_BY_LENGTH_FIELD;
    m_unpackSetting.body_offset = 0;              // 数据体偏移（长度字段在开头，所以为0）
    m_unpackSetting.length_field_offset = 0;      // 长度字段在数据包开头
    m_unpackSetting.length_field_bytes = 4;       // 长度字段占4字节
    m_unpackSetting.length_adjustment = 0;        // 长度字段值已经包含自身4字节，不需要调整
    m_unpackSetting.length_field_coding = ENCODE_BY_BIG_ENDIAN;  // 网络字节序（大端）
    m_unpackSetting.package_max_length = 64 * 1024; // 最大包长度64KB
}

void TcpTransport::setupClient() {
    if (!m_client) {
        return;
    }
    
    // 设置拆包配置
    initUnpackSetting();
    m_client->setUnpack(&m_unpackSetting);
    
    // // 配置自动重连机制
    // reconn_setting_t reconn;
    // reconn_setting_init(&reconn);
    // reconn.min_delay = 500;   // 最小重连延迟
    // reconn.max_delay = 5000;  // 最大重连延迟
    // reconn.delay_policy = 0;   // 指数退避策略
    // m_client->setReconnect(&reconn);
    
    // 设置连接回调
    m_client->onConnection = [this](const hv::SocketChannelPtr& channel) {
        if (channel->isConnected()) {
            m_connected.store(true, boost::memory_order_release);
            LOG_DEBUG("TcpTransport: Connected to " + channel->peeraddr());
            
            // 连接建立时统一设置超时
            channel->setReadTimeout(m_readTimeout);
            channel->setWriteTimeout(m_writeTimeout);
            channel->setCloseTimeout(m_closeTimeout);
            channel->setKeepaliveTimeout(m_keepaliveTimeout);
            
            m_connectionCondition.notify_all();
        } else {
            m_connected.store(false, boost::memory_order_release);
            m_connectFailed.store(true, boost::memory_order_release);
            LOG_DEBUG("TcpTransport: Disconnected from " + channel->peeraddr());
            m_connectionCondition.notify_all();
        }
    };
    
    // 设置消息回调
    m_client->onMessage = [this](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
        boost::lock_guard<boost::mutex> lock(m_bufferMutex);
        const uint8_t* data = reinterpret_cast<const uint8_t*>(buf->data());
        std::vector<uint8_t> packet(data, data + buf->size());
        m_packetQueue.push(packet);
        m_packetCondition.notify_one();
        LOG_DEBUG("TcpTransport: Received complete packet, size=" + std::to_string(buf->size()));
    };
}

int TcpTransport::sendRequest(const Request& req, Response& resp) {
    LOG_DEBUG("TcpTransport::sendRequest: Starting, URL=" + req.url);
    
    // 解析 URL 获取 host 和 port
    std::string host;
    int port;
    if (!parseUrl(req.url, host, port)) {
        resp.statusCode = -1;
        resp.errorMessage = "Invalid URL format: " + req.url;
        LOG_ERROR("TcpTransport::sendRequest: Invalid URL format: " + req.url);
        return -1;
    }
    LOG_DEBUG("TcpTransport::sendRequest: Parsed URL - host=" + host + ", port=" + std::to_string(port));
    
    // 检查是否需要重新连接或重新创建客户端
    bool needReconnect = false;
    bool needRecreate = false;
    {
        boost::lock_guard<boost::mutex> lock(m_clientMutex);
        
        // 如果客户端被重置，需要重新创建
        if (!m_client) {
            needRecreate = true;
        } else {
            // 检查是否需要重新连接
            needReconnect = !m_connected.load(boost::memory_order_acquire) || 
                            m_currentHost != host || m_currentPort != port;
        }
    }
    // 释放锁
    
    // 如果需要重新创建客户端（例如 disconnect() 后 m_client 被重置）
    if (needRecreate) {
        boost::lock_guard<boost::mutex> lock(m_clientMutex);
        
        try {
            m_client = std::unique_ptr<hv::TcpClient>(new hv::TcpClient());
            if (!m_client) {
                resp.statusCode = -1;
                resp.errorMessage = "Failed to create TcpClient";
                LOG_ERROR("TcpTransport::sendRequest: Failed to create TcpClient");
                return -1;
            }
            
            // 重新设置客户端配置（拆包、重连、回调等）
            setupClient();
            m_connected.store(false, boost::memory_order_release);
            needReconnect = true;  // 创建后需要连接
        } catch (const std::exception& e) {
            resp.statusCode = -1;
            resp.errorMessage = "Failed to recreate TcpClient: " + std::string(e.what());
            LOG_ERROR("TcpTransport::sendRequest: Failed to recreate TcpClient: " + std::string(e.what()));
            return -1;
        }
    }
    
    // 如果需要重新连接
    if (needReconnect) {
        LOG_DEBUG("TcpTransport::sendRequest: Need to reconnect to " + host + ":" + std::to_string(port));
        
        boost::lock_guard<boost::mutex> lock(m_clientMutex);
        
        disconnectInternal();
        
        // 设置连接超时（这会设置 TcpClient 内部的 connect_timeout 成员变量）
        m_client->setConnectTimeout(m_connectTimeout);
        m_connectFailed.store(false, boost::memory_order_release);
        
        // 创建socket并连接
        LOG_DEBUG("TcpTransport::sendRequest: Creating socket...");
        int connfd = m_client->createsocket(port, host.c_str());
        if (connfd < 0) {
            resp.statusCode = -1;
            resp.errorMessage = "Failed to create socket for " + host + ":" + std::to_string(port);
            LOG_ERROR("TcpTransport::sendRequest: Failed to create socket, connfd=" + std::to_string(connfd));
            return -1;
        }
        LOG_DEBUG("TcpTransport::sendRequest: Socket created, connfd=" + std::to_string(connfd));
        
        // 重要：手动设置 channel 的连接超时
        // libhv 的 startConnect() 会跳过已存在的 channel，所以我们需要手动设置
        if (m_client->channel) {
            m_client->channel->setConnectTimeout(m_connectTimeout);
            LOG_DEBUG("TcpTransport::sendRequest: Set channel connect timeout to " + std::to_string(m_connectTimeout) + " ms");
        }
        
        m_currentHost = host;
        m_currentPort = port;
        
        LOG_DEBUG("TcpTransport::sendRequest: Starting client...");
        m_client->start();
        LOG_DEBUG("TcpTransport::sendRequest: Client started, waiting for connection...");
    }
    // 锁已释放，避免在等待连接时持有锁
    
    // 等待连接建立（使用条件变量，不持有锁）
    {
        boost::unique_lock<boost::mutex> lock(m_clientMutex);
        auto startTime = boost::chrono::steady_clock::now();
        
        while (!m_connected.load(boost::memory_order_acquire)) {
            // 检查是否连接失败（被回调通知）
            if (m_connectFailed.load(boost::memory_order_acquire)) {
                resp.statusCode = -1;
                resp.errorMessage = "Connection refused/failed to " + host + ":" + std::to_string(port);
                LOG_ERROR("TcpTransport::sendRequest: Connection failed (notified)");
                return -1;
            }
            
            // 使用条件变量等待，避免循环轮询
            (void)m_connectionCondition.wait_for(
                lock, 
                boost::chrono::milliseconds(100)
            );
            
            // 检查超时（使用毫秒）
            auto now = boost::chrono::steady_clock::now();
            auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(now - startTime).count();
            
            // 确定连接超时时间（优先使用请求中的超时，否则使用默认值）
            // 注意：req.connect_timeout = -1 表示使用默认值，0 也表示使用默认值（0 是无效的超时值）
            int connectTimeout = (req.connect_timeout >= 0) ? req.connect_timeout : m_connectTimeout;
            
            // 只有当 connectTimeout > 0 时才检查超时（0 表示不设置超时，无限等待）
            if (connectTimeout > 0 && elapsed >= connectTimeout) {
                resp.statusCode = -1;
                resp.errorMessage = "Connection timeout to " + host + ":" + std::to_string(port);
                LOG_ERROR("TcpTransport::sendRequest: Connection timeout after " + std::to_string(elapsed) + " ms");
                disconnectInternal();
                return -1;
            }
            
            // 检查客户端连接状态（libhv 可能已连接但回调未触发）
            if (m_client && m_client->isConnected()) {
                LOG_DEBUG("TcpTransport::sendRequest: Client connected (detected via isConnected())");
                m_connected.store(true, boost::memory_order_release);
                break;
            }
        }
    }
    
    if (!m_connected.load(boost::memory_order_acquire)) {
        resp.statusCode = -1;
        resp.errorMessage = "Connection failed to " + host + ":" + std::to_string(port);
        LOG_ERROR("TcpTransport::sendRequest: Connection failed");
        return -1;
    }
    
    LOG_DEBUG("TcpTransport::sendRequest: Connected successfully");
    
    try {
        // 构造数据包（使用头部长度字段格式）
        // 格式：4字节长度（网络字节序）+ 数据
        // Guard against body size exceeding uint32 range (CWE-190)
        if (req.body.size() > 0xFFFFFFFBu) { // max uint32 - 4
            resp.statusCode = -1;
            resp.errorMessage = "Body too large for TCP frame";
            LOG_ERROR("TcpTransport::sendRequest: Body size exceeds uint32 limit");
            return -1;
        }
        uint32_t dataLen = static_cast<uint32_t>(req.body.size());
        uint32_t totalLen = htonl(dataLen + 4);  // 总长度包含4字节长度字段
        
        // 发送数据（先发送长度字段，再发送数据）
        std::vector<uint8_t> sendBuffer;
        sendBuffer.resize(4 + req.body.size());
        memcpy(sendBuffer.data(), &totalLen, 4);
        if (!req.body.empty()) {
            memcpy(sendBuffer.data() + 4, req.body.data(), req.body.size());
        }
        
        int sent;
        {
            boost::lock_guard<boost::mutex> lock(m_clientMutex);
            if (!m_client) {
                resp.statusCode = -1;
                resp.errorMessage = "Client destroyed during send";
                LOG_ERROR("TcpTransport::sendRequest: Client destroyed");
                return -1;
            }
            sent = m_client->send(sendBuffer.data(), sendBuffer.size());
        }
        
        if (sent < 0) {
            resp.statusCode = -1;
            resp.errorMessage = "Failed to send data";
            LOG_ERROR("TcpTransport::sendRequest: Failed to send data");
            m_connected.store(false, boost::memory_order_release);
            return -1;
        }
        
        LOG_DEBUG("TcpTransport::sendRequest: Sent " + std::to_string(sent) + " bytes");
        
        // 检查连接状态
        if (!m_connected.load(boost::memory_order_acquire)) {
            resp.statusCode = -1;
            resp.errorMessage = "Connection lost before receiving response";
            LOG_ERROR("TcpTransport::sendRequest: Connection lost");
            return -1;
        }
        
        // 如果请求中指定了超时，临时覆盖相应的超时配置
        if (m_client) {
            hv::SocketChannelPtr channel = m_client->channel;
            if (!channel) {
                LOG_WARN("TcpTransport::sendRequest: Channel is null, skipping timeout update");
                return receiveResponseWithTimeout(resp, m_readTimeout);
            }
            
            if (req.read_timeout >= 0) {
                channel->setReadTimeout(req.read_timeout);
            }
            if (req.write_timeout >= 0) {
                channel->setWriteTimeout(req.write_timeout);
            }
            if (req.close_timeout >= 0) {
                channel->setCloseTimeout(req.close_timeout);
            }
            if (req.keepalive_timeout >= 0) {
                channel->setKeepaliveTimeout(req.keepalive_timeout);
            }
        }
        
        // 等待接收响应（使用请求中的读取超时，如果没有设置则使用默认的读取超时）
        int readTimeout = (req.read_timeout >= 0) ? req.read_timeout : m_readTimeout;
        LOG_DEBUG("TcpTransport::sendRequest: Waiting for response (readTimeout=" + std::to_string(readTimeout) + "ms)");
        return receiveResponseWithTimeout(resp, readTimeout);
        
    } catch (const std::exception& e) {
        resp.statusCode = -1;
        resp.errorMessage = std::string("Exception: ") + e.what();
        LOG_ERROR("TcpTransport::sendRequest exception: " + resp.errorMessage + ", URL=" + req.url);
        m_connected.store(false, boost::memory_order_release);
        return -1;
    } catch (...) {
        resp.statusCode = -1;
        resp.errorMessage = "Unknown exception";
        LOG_ERROR("TcpTransport::sendRequest unknown exception, URL=" + req.url);
        m_connected.store(false, boost::memory_order_release);
        return -1;
    }
}

int TcpTransport::receiveResponse(Response& resp) {
    // 使用默认读取超时（毫秒）
    return receiveResponseWithTimeout(resp, m_readTimeout);
}

int TcpTransport::receiveResponseWithTimeout(Response& resp, int timeoutMilliseconds) {
    LOG_DEBUG("TcpTransport::receiveResponseWithTimeout: Entering, timeout=" + std::to_string(timeoutMilliseconds) + "ms");
    
    // 检查连接状态
    if (!m_connected.load(boost::memory_order_acquire)) {
        resp.statusCode = -1;
        resp.errorMessage = "Not connected, cannot receive response";
        LOG_ERROR("TcpTransport::receiveResponseWithTimeout: Not connected");
        return -1;
    }
    
    // 如果超时时间为0或负数，立即返回
    if (timeoutMilliseconds <= 0) {
        resp.statusCode = -1;
        resp.errorMessage = "Invalid timeout value";
        LOG_ERROR("TcpTransport::receiveResponseWithTimeout: Invalid timeout value: " + std::to_string(timeoutMilliseconds));
        return -1;
    }
    
    boost::unique_lock<boost::mutex> lock(m_bufferMutex);
    
    // 等待完整数据包（libhv的拆包功能会自动处理）
    if (m_packetQueue.empty()) {
        LOG_DEBUG("TcpTransport::receiveResponseWithTimeout: Packet queue empty, waiting...");
        // 设置超时（使用毫秒）
        auto startWait = boost::chrono::steady_clock::now();
        if (m_packetCondition.wait_for(lock, boost::chrono::milliseconds(timeoutMilliseconds)) == boost::cv_status::timeout) {
            auto endWait = boost::chrono::steady_clock::now();
            auto waitTime = boost::chrono::duration_cast<boost::chrono::milliseconds>(endWait - startWait).count();
            resp.statusCode = -1;
            resp.errorMessage = "Timeout waiting for response";
            LOG_ERROR("TcpTransport::receiveResponseWithTimeout: Timeout after " + std::to_string(waitTime) + "ms");
            return -1;
        }
        LOG_DEBUG("TcpTransport::receiveResponseWithTimeout: Condition notified");
    }
    
    if (m_packetQueue.empty()) {
        resp.statusCode = -1;
        resp.errorMessage = "No packet available";
        LOG_ERROR("TcpTransport::receiveResponseWithTimeout: No packet available");
        return -1;
    }
    
    // 获取数据包（libhv已经完成拆包，这里获取的是完整数据包）
    // 注意：数据包包含4字节长度字段+数据，需要跳过长度字段
    std::vector<uint8_t> packet = m_packetQueue.front();
    m_packetQueue.pop();
    
    if (packet.size() < 4) {
        resp.statusCode = -1;
        resp.errorMessage = "Invalid packet size";
        LOG_ERROR("TcpTransport::receiveResponseWithTimeout: Invalid packet size: " + std::to_string(packet.size()));
        return -1;
    }
    
    // 跳过4字节长度字段，获取实际数据
    resp.statusCode = 200;
    resp.body.assign(packet.begin() + 4, packet.end());
    
    LOG_DEBUG("TcpTransport::receiveResponseWithTimeout: Received response, size=" + std::to_string(resp.body.size()));
    return 0;
}

bool TcpTransport::connect() {
    // TCP 连接在 sendRequest 时建立（每次请求建立新连接）
    return true;  // 总是返回 true，因为连接在发送时建立
}

void TcpTransport::disconnect() {
    LOG_DEBUG("TcpTransport::disconnect: Disconnecting...");

    std::unique_ptr<hv::TcpClient> clientToClose;
    bool needStop = false;
    {
        boost::unique_lock<boost::mutex> lock(m_clientMutex);
        if (m_client) {
            needStop = !m_currentHost.empty() || m_currentPort != 0;
            // 析构/主动断开前摘掉捕获 this 的回调，再把 client 移出成员。
            m_client->onConnection = nullptr;
            m_client->onMessage = nullptr;
            clientToClose = std::move(m_client);
        }
        m_connected.store(false, boost::memory_order_release);
        m_connectFailed.store(true, boost::memory_order_release);
        m_currentHost.clear();
        m_currentPort = 0;
    }

    if (clientToClose) {
        if (needStop) {
            try {
                LOG_DEBUG("TcpTransport::disconnect: Calling stop() without lock...");
                clientToClose->stop();
                LOG_DEBUG("TcpTransport::disconnect: stop() returned");
                
                clientToClose->closesocket();
                LOG_DEBUG("TcpTransport::disconnect: closesocket() returned");
            } catch (const std::exception& e) {
                LOG_ERROR("TcpTransport::disconnect: Exception during stop/close: " + std::string(e.what()));
            } catch (...) {
                LOG_ERROR("TcpTransport::disconnect: Unknown exception during stop/close");
            }
        } else {
            LOG_DEBUG("TcpTransport::disconnect: Client never started, skipping stop()");
        }
        LOG_DEBUG("TcpTransport::disconnect: Client reset");
    }

    // 清空接收队列并通知等待的线程
    {
        boost::lock_guard<boost::mutex> bufferLock(m_bufferMutex);
        while (!m_packetQueue.empty()) {
            m_packetQueue.pop();
        }
        m_packetCondition.notify_all(); // 通知所有等待的线程，避免卡住
    }
    
    // 通知等待连接的线程
    m_connectionCondition.notify_all();
    
    LOG_DEBUG("TcpTransport::disconnect: Disconnected and queue cleared");
}

void TcpTransport::disconnectInternal() {
    // 注意：此方法假设调用者已持有 m_clientMutex 锁（unique_lock）
    LOG_DEBUG("TcpTransport::disconnectInternal: Disconnecting...");
    
    if (m_client) {
        // 先设置连接状态为false
        m_connected.store(false, boost::memory_order_release);
        
        // 需要临时释放锁来调用 stop()
        // 由于调用者持有 unique_lock，需要将锁传入或在外部处理
        // 这里简化为直接调用 closesocket（不调用 stop）
        try {
            LOG_DEBUG("TcpTransport::disconnectInternal: Calling closesocket()...");
            m_client->closesocket();
            LOG_DEBUG("TcpTransport::disconnectInternal: closesocket() returned");
        } catch (const std::exception& e) {
            LOG_ERROR("TcpTransport::disconnectInternal: Exception during close: " + std::string(e.what()));
        } catch (...) {
            LOG_ERROR("TcpTransport::disconnectInternal: Unknown exception during close");
        }
        
        // 注意：disconnectInternal 在 sendRequest 中调用，不重置 m_client
        // 因为 sendRequest 可能需要重新使用 m_client 进行重连
        // 只有在 disconnect() 中才真正重置 m_client
    }
    
    m_currentHost.clear();
    m_currentPort = 0;
    
    // 清空接收队列并通知等待的线程
    {
        boost::lock_guard<boost::mutex> bufferLock(m_bufferMutex);
        while (!m_packetQueue.empty()) {
            m_packetQueue.pop();
        }
        m_packetCondition.notify_all(); // 通知所有等待的线程，避免卡住
    }
    
    // 通知等待连接的线程
    m_connectionCondition.notify_all();
    
    LOG_DEBUG("TcpTransport::disconnectInternal: Disconnected and queue cleared");
}

bool TcpTransport::isConnected() const {
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

void TcpTransport::setConnectTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_connectTimeout = milliseconds;
}

void TcpTransport::setReadTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_readTimeout = milliseconds;
}

void TcpTransport::setWriteTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_writeTimeout = milliseconds;
}

void TcpTransport::setCloseTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_closeTimeout = milliseconds;
}

void TcpTransport::setKeepaliveTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_keepaliveTimeout = milliseconds;
}

std::string TcpTransport::getTransportType() const {
    return "tcp";
}

} // namespace fw
} // namespace intertwine
