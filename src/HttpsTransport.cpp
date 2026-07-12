#include "intertwine/fw/HttpsTransport.hpp"
#include "intertwine/fw/Log.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/system/error_code.hpp>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <hv/hssl.h>
#include <hv/hloop.h>

// 包含 libhv 的配置头文件以获取 WITH_OPENSSL 定义
#include <hv/hconfig.h>

// 如果使用 OpenSSL，尝试通过底层 API 设置 SSL 版本
#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#endif

namespace intertwine {
namespace fw {

HttpsTransport::HttpsTransport()
    : m_timeout(30000)
    , m_connectTimeout(3000)
    , m_readTimeout(30000)
    , m_writeTimeout(30000)
    , m_closeTimeout(5000)
    , m_keepaliveTimeout(60000)
    , m_connected(false)
{
    // 默认SSL配置
    m_sslConfig.verifyPeer = true;
    m_sslConfig.verifyHost = true;
    m_sslConfig.sslVersion = "TLS1.2";
    m_sslConfig.sessionReuse = true;
    
    try {
        m_client = std::unique_ptr<hv::HttpClient>(new hv::HttpClient());
        if (m_client) {
            configureSsl();
            m_connected = false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create HttpClient: " + std::string(e.what()));
        m_connected = false;
    }
}

HttpsTransport::HttpsTransport(const SslConfig& sslConfig)
    : m_sslConfig(sslConfig)
    , m_timeout(30000)
    , m_connectTimeout(3000)
    , m_readTimeout(30000)
    , m_writeTimeout(30000)
    , m_closeTimeout(5000)
    , m_keepaliveTimeout(60000)
    , m_connected(false)
{
    try {
        m_client = std::unique_ptr<hv::HttpClient>(new hv::HttpClient());
        if (m_client) {
            configureSsl();
            m_connected = false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create HttpClient: " + std::string(e.what()));
        m_connected = false;
    }
}

HttpsTransport::~HttpsTransport() {
    try {
        disconnect(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    } catch (...) {}
}

void HttpsTransport::configureSsl() {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    
    if (!m_client) {
        return;
    }
    
    try {
        // 配置 SSL（使用 libhv 的 newSslCtx API）
        hssl_ctx_opt_t ssl_opt;
        memset(&ssl_opt, 0, sizeof(ssl_opt));
        
        // 设置客户端模式
        ssl_opt.endpoint = HSSL_CLIENT;
        
        // 设置是否验证服务器证书
        ssl_opt.verify_peer = m_sslConfig.verifyPeer ? 1 : 0;
        
        // 注意：libhv 的 hssl_ctx_opt_t 不支持 verifyHost（主机名验证）
        // verifyHost 配置项被读取但无法直接设置
        // libhv 可能在某些情况下自动进行主机名验证，但无法通过配置控制
        if (m_sslConfig.verifyHost) {
            LOG_WARN("HttpsTransport: verifyHost is set to true, but libhv does not support "
                    "explicit hostname verification configuration. Hostname verification may "
                    "be performed automatically by libhv in some cases.");
        }
        
        // 注意：libhv 的 hssl_ctx_opt_t 结构体没有 sslVersion 字段
        // 但如果使用 OpenSSL，可以在创建 SSL 上下文后通过 OpenSSL API 设置版本
        // 这里先记录配置，稍后在应用 SSL 配置后尝试设置
        
        // 注意：libhv 的 hssl_ctx_opt_t 不支持 sessionReuse（会话复用）
        // sessionReuse 配置项被读取但无法直接设置
        // libhv 可能默认启用会话复用，但无法通过配置控制
        if (!m_sslConfig.sessionReuse) {
            LOG_WARN("HttpsTransport: sessionReuse is set to false, but libhv does not support "
                    "explicit session reuse configuration. Session reuse may be enabled by default.");
        }
        
        // 设置 CA 证书文件或路径
        if (!m_sslConfig.caCertPath.empty()) {
            // 判断是文件还是目录
            if (m_sslConfig.caCertPath.back() == '/') {
                ssl_opt.ca_path = m_sslConfig.caCertPath.c_str();
            } else {
                ssl_opt.ca_file = m_sslConfig.caCertPath.c_str();
            }
            LOG_DEBUG("HTTPS CA certificate set: " + m_sslConfig.caCertPath);
        }
        
        // 设置客户端证书
        if (!m_sslConfig.clientCertPath.empty()) {
            ssl_opt.crt_file = m_sslConfig.clientCertPath.c_str();
            LOG_DEBUG("HTTPS client certificate set: " + m_sslConfig.clientCertPath);
        }
        
        // 设置客户端私钥
        if (!m_sslConfig.clientKeyPath.empty()) {
            ssl_opt.key_file = m_sslConfig.clientKeyPath.c_str();
            LOG_DEBUG("HTTPS client key set: " + m_sslConfig.clientKeyPath);
        }
        
        // 应用 SSL 配置
        // 如果需要设置 SSL 版本，先通过 hssl_ctx_new 创建上下文，设置版本后再应用
        #ifdef WITH_OPENSSL
        // 如果配置了 SSL 版本，尝试通过 OpenSSL API 设置
        bool needVersionSetting = !m_sslConfig.sslVersion.empty();
        
        if (needVersionSetting) {
            // 先创建 SSL 上下文
            hssl_ctx_t ssl_ctx = hssl_ctx_new(&ssl_opt);
            if (ssl_ctx) {
                // 尝试通过 OpenSSL API 设置版本
                // 注意：hssl_ctx_t 在使用 OpenSSL 时就是 SSL_CTX*
                SSL_CTX* ctx = reinterpret_cast<SSL_CTX*>(ssl_ctx);
                
                std::string versionUpper = boost::to_upper_copy(m_sslConfig.sslVersion);
                long version = 0;
                
                // 解析版本字符串
                if (versionUpper == "TLS1.0" || versionUpper == "TLS1_0") {
                    version = TLS1_VERSION;
                } else if (versionUpper == "TLS1.1" || versionUpper == "TLS1_1") {
                    version = TLS1_1_VERSION;
                } else if (versionUpper == "TLS1.2" || versionUpper == "TLS1_2") {
                    version = TLS1_2_VERSION;
                } else if (versionUpper == "TLS1.3" || versionUpper == "TLS1_3") {
                    #if defined(TLS1_3_VERSION)
                    version = TLS1_3_VERSION;
                    #else
                    LOG_WARN("HttpsTransport: TLS 1.3 is not supported in this OpenSSL version");
                    #endif
                }
                
                if (version > 0) {
                    // 设置最小和最大版本为相同值，强制使用指定版本
                    #if OPENSSL_VERSION_NUMBER >= 0x10100000L
                    // OpenSSL 1.1.0+ 使用新 API
                    if (SSL_CTX_set_min_proto_version(ctx, version) == 1 &&
                        SSL_CTX_set_max_proto_version(ctx, version) == 1) {
                        LOG_INFO("HttpsTransport: SSL/TLS version set to " + m_sslConfig.sslVersion);
                    } else {
                        LOG_WARN("HttpsTransport: Failed to set SSL/TLS version to " + m_sslConfig.sslVersion);
                    }
                    #else
                    // OpenSSL 1.0.x 使用旧 API
                    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
                    if (version == TLS1_VERSION) {
                        SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2);
                    } else if (version == TLS1_1_VERSION) {
                        SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_2);
                    } else if (version == TLS1_2_VERSION) {
                        SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
                    }
                    LOG_INFO("HttpsTransport: SSL/TLS version set to " + m_sslConfig.sslVersion + 
                            " (using OpenSSL 1.0.x API)");
                    #endif
                } else {
                    // 版本解析失败，记录警告但继续使用默认配置
                    LOG_WARN("HttpsTransport: Unknown SSL version '" + m_sslConfig.sslVersion + 
                            "', using system default. Supported versions: TLS1.0, TLS1.1, TLS1.2, TLS1.3");
                }
                
                // 将配置好的 SSL 上下文设置到 HttpClient
                int ret = m_client->setSslCtx(ssl_ctx);
                if (ret != 0) {
                    LOG_ERROR("Failed to set SSL context, ret=" + std::to_string(ret));
                    hssl_ctx_free(ssl_ctx);
                } else {
                    LOG_INFO("HTTPS SSL configuration applied successfully");
                    if (m_sslConfig.verifyPeer) {
                        LOG_DEBUG("SSL peer verification: enabled");
                    } else {
                        LOG_WARN("SSL peer verification: disabled (not recommended for production)");
                    }
                }
            } else {
                LOG_ERROR("Failed to create SSL context for version setting");
                // 回退到使用 newSslCtx
                int ret = m_client->newSslCtx(&ssl_opt);
                if (ret != 0) {
                    LOG_ERROR("Failed to create SSL context, ret=" + std::to_string(ret));
                }
            }
        } else {
            // 不需要设置版本，直接使用 newSslCtx
            int ret = m_client->newSslCtx(&ssl_opt);
            if (ret != 0) {
                LOG_ERROR("Failed to create SSL context, ret=" + std::to_string(ret));
            } else {
                LOG_INFO("HTTPS SSL configuration applied successfully");
                if (m_sslConfig.verifyPeer) {
                    LOG_DEBUG("SSL peer verification: enabled");
                } else {
                    LOG_WARN("SSL peer verification: disabled (not recommended for production)");
                }
            }
        }
        #else
        // 非 OpenSSL 后端，无法设置版本
        int ret = m_client->newSslCtx(&ssl_opt);
        if (ret != 0) {
            LOG_ERROR("Failed to create SSL context, ret=" + std::to_string(ret));
        } else {
            LOG_INFO("HTTPS SSL configuration applied successfully");
            if (m_sslConfig.verifyPeer) {
                LOG_DEBUG("SSL peer verification: enabled");
            } else {
                LOG_WARN("SSL peer verification: disabled (not recommended for production)");
            }
            if (!m_sslConfig.sslVersion.empty() && m_sslConfig.sslVersion != "TLS1.2") {
                LOG_WARN("HttpsTransport: sslVersion is set to '" + m_sslConfig.sslVersion + 
                        "', but libhv does not support explicit SSL/TLS version configuration "
                        "for non-OpenSSL backends. Using system default SSL/TLS version.");
            }
        }
        #endif
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to configure SSL: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Failed to configure SSL: unknown exception");
    }
}

bool HttpsTransport::parseUrl(const std::string& url, 
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
        // 没有 scheme，默认为 https
        scheme = "https";
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
        if (scheme == "https") {
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

void HttpsTransport::parseHeaders(const std::string& headerStr,
                                 std::map<std::string, std::string>& headers) {
    if (headerStr.empty()) {
        return;
    }
    
    std::vector<std::string> lines;
    boost::split(lines, headerStr, boost::is_any_of("\r\n"), boost::token_compress_on);
    
    for (const std::string& line : lines) {
        if (line.empty()) {
            continue;
        }
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            boost::trim(key);
            boost::trim(value);
            
            if (!key.empty() && !value.empty()) {
                // 转换为小写键名（HTTP 头不区分大小写，但统一小写便于管理）
                std::string lowerKey = boost::to_lower_copy(key);
                headers[lowerKey] = value;
            }
        }
    }
}

std::string HttpsTransport::buildHeadersString(
    const std::map<std::string, std::string>& headers) {
    std::ostringstream oss;
    for (const auto& pair : headers) {
        oss << pair.first << ": " << pair.second << "\r\n";
    }
    return oss.str();
}

int HttpsTransport::sendRequest(const Request& req, Response& resp) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    
    if (!m_client) {
        resp.statusCode = -1;
        resp.errorMessage = "HttpClient not initialized";
        LOG_ERROR("HttpsTransport::sendRequest: HttpClient not initialized");
        return -1;
    }
    
    try {
        // 解析 URL
        std::string scheme, host, path;
        int port;
        if (!parseUrl(req.url, scheme, host, port, path)) {
            resp.statusCode = -1;
            resp.errorMessage = "Invalid URL format: " + req.url;
            LOG_ERROR("HttpsTransport::sendRequest: Invalid URL format: " + req.url);
            return -1;
        }
        
        // 支持 HTTP 和 HTTPS
        if (scheme != "http" && scheme != "https") {
            resp.statusCode = -1;
            resp.errorMessage = "Only HTTP and HTTPS schemes are supported";
            LOG_ERROR("HttpsTransport::sendRequest: Unsupported scheme: " + scheme);
            return -1;
        }
        
        // 创建 libhv 请求对象
        HttpRequest hvReq;
        hvReq.url = req.url;
        
        // 设置 HTTP 方法
        if (req.method.empty()) {
            hvReq.method = HTTP_GET;
        } else {
            std::string methodUpper = boost::to_upper_copy(req.method);
            if (methodUpper == "GET") {
                hvReq.method = HTTP_GET;
            } else if (methodUpper == "POST") {
                hvReq.method = HTTP_POST;
            } else if (methodUpper == "PUT") {
                hvReq.method = HTTP_PUT;
            } else if (methodUpper == "DELETE") {
                hvReq.method = HTTP_DELETE;
            } else if (methodUpper == "HEAD") {
                hvReq.method = HTTP_HEAD;
            } else if (methodUpper == "OPTIONS") {
                hvReq.method = HTTP_OPTIONS;
            } else {
                hvReq.method = http_method_enum(req.method.c_str());
            }
        }
        
        // 解析并设置请求头
        if (!req.headers.empty()) {
            std::map<std::string, std::string> headers;
            parseHeaders(req.headers, headers);
            for (const auto& pair : headers) {
                hvReq.headers[pair.first] = pair.second;
            }
        }
        
        // 设置请求体
        if (!req.body.empty()) {
            hvReq.body = req.body;
        }
        
        // 计算超时值（优先使用请求中的超时，否则使用默认超时）
        // 注意：HttpRequest 只支持 timeout 和 connect_timeout字段（单位：秒，uint16_t 类型）
        int timeout = (req.timeout >= 0) ? req.timeout : m_timeout;
        int connectTimeout = (req.connect_timeout >= 0) ? req.connect_timeout : m_connectTimeout;
        // 设置 HTTPS 请求超时（单位：秒）
        hvReq.timeout = static_cast<uint16_t>(std::min(timeout / 1000, 65535));  // 转换为秒
        hvReq.connect_timeout = static_cast<uint16_t>(std::min(connectTimeout / 1000, 65535));  // 转换为秒
        // 发送请求
        HttpResponse hvHttpResponse;
        int ret = m_client->send(&hvReq, &hvHttpResponse);
        
        if (ret != 0) {
            resp.statusCode = -1;
            resp.errorMessage = "Request failed with code: " + std::to_string(ret) 
                              + ", URL: " + req.url;
            LOG_ERROR("HttpsTransport::sendRequest: Request failed, ret=" + std::to_string(ret) 
                     + ", URL=" + req.url);
            // 连接可能已断开，标记为未连接
            m_connected = false;
            return -1;
        }
        
        // 转换响应
        resp.statusCode = hvHttpResponse.status_code;
        resp.body = hvHttpResponse.body;
        // 转换headers（libhv的headers类型可能不同，需要手动转换）
        std::ostringstream headerOss;
        for (const auto& pair : hvHttpResponse.headers) {
            headerOss << pair.first << ": " << pair.second << "\r\n";
        }
        resp.headers = headerOss.str();
        
        // 如果状态码表示错误，设置错误消息
        if (resp.statusCode >= 400) {
            // 从响应头中获取错误信息
            std::string reason = "Request failed";
            auto it = hvHttpResponse.headers.find("reason");
            if (it != hvHttpResponse.headers.end()) {
                reason = it->second;
            } else {
                auto it2 = hvHttpResponse.headers.find("Reason-Phrase");
                if (it2 != hvHttpResponse.headers.end()) {
                    reason = it2->second;
                }
            }
            resp.errorMessage = "HTTP " + std::to_string(resp.statusCode) + ": " + reason;
            LOG_WARN("HttpsTransport::sendRequest: HTTP error " + std::to_string(resp.statusCode) 
                    + ": " + reason + ", URL=" + req.url);
        } else {
            LOG_DEBUG("HttpsTransport::sendRequest: Success, status=" + std::to_string(resp.statusCode) 
                     + ", body_size=" + std::to_string(resp.body.size()) + ", URL=" + req.url);
        }
        
        m_connected = true;
        return 0;
        
    } catch (const std::exception& e) {
        resp.statusCode = -1;
        resp.errorMessage = std::string("Exception: ") + e.what();
        LOG_ERROR("HttpsTransport::sendRequest exception: " + resp.errorMessage + ", URL=" + req.url);
        m_connected = false;
        return -1;
    } catch (...) {
        resp.statusCode = -1;
        resp.errorMessage = "Unknown exception";
        LOG_ERROR("HttpsTransport::sendRequest unknown exception, URL=" + req.url);
        m_connected = false;
        return -1;
    }
}

int HttpsTransport::receiveResponse(Response& resp) {
    // libhv 同步 API 在 sendRequest 中已经完成
    // 这里可以返回上次的响应或实现异步接收
    if (m_connected) {
        return 0;
    }
    resp.statusCode = -1;
    resp.errorMessage = "No active connection";
    return -1;
}

bool HttpsTransport::connect() {
    // libhv HttpClient 不需要显式连接
    // 在 sendRequest 时自动连接
    return m_connected && m_client != nullptr;
}

void HttpsTransport::disconnect() {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_client.reset();
    m_connected = false;
}

bool HttpsTransport::isConnected() const {
    return m_connected && m_client != nullptr;
}

std::string HttpsTransport::getTransportType() const {
    return "https";
}

void HttpsTransport::setSslConfig(const SslConfig& config) {
    m_sslConfig = config;
    configureSsl();
}

void HttpsTransport::setTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_timeout = milliseconds;
}

void HttpsTransport::setConnectTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_connectTimeout = milliseconds;
}

void HttpsTransport::setReadTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_readTimeout = milliseconds;
}

void HttpsTransport::setWriteTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_writeTimeout = milliseconds;
}

void HttpsTransport::setCloseTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_closeTimeout = milliseconds;
}

void HttpsTransport::setKeepaliveTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_keepaliveTimeout = milliseconds;
}

} // namespace fw
} // namespace intertwine
