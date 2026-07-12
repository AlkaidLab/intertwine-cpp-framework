#include "intertwine/fw/HttpTransport.hpp"
#include "intertwine/fw/Log.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <hv/hloop.h>
#include <algorithm>
#include <sstream>

namespace intertwine {
namespace fw {

HttpTransport::HttpTransport()
    : m_timeout(30000)
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
            m_connected = false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create HttpClient: " + std::string(e.what()));
        m_connected = false;
    }
}

HttpTransport::~HttpTransport() {
    try {
        disconnect(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    } catch (...) {}
}

bool HttpTransport::parseUrl(const std::string& url, 
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
        // 没有 scheme，默认为 http
        scheme = "http";
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

void HttpTransport::parseHeaders(const std::string& headerStr,
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

std::string HttpTransport::buildHeadersString(
    const std::map<std::string, std::string>& headers) {
    std::ostringstream oss;
    for (const auto& pair : headers) {
        oss << pair.first << ": " << pair.second << "\r\n";
    }
    return oss.str();
}

int HttpTransport::sendRequest(const Request& req, Response& resp) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    
    if (!m_client) {
        resp.statusCode = -1;
        resp.errorMessage = "HttpClient not initialized";
        LOG_ERROR("HttpTransport::sendRequest: HttpClient not initialized");
        return -1;
    }
    
    try {
        // 解析 URL
        std::string scheme, host, path;
        int port;
        if (!parseUrl(req.url, scheme, host, port, path)) {
            resp.statusCode = -1;
            resp.errorMessage = "Invalid URL format: " + req.url;
            LOG_ERROR("HttpTransport::sendRequest: Invalid URL format: " + req.url);
            return -1;
        }
        
        // 只支持 HTTP
        if (scheme != "http") {
            resp.statusCode = -1;
            resp.errorMessage = "Only HTTP scheme is supported (use HttpsTransport for HTTPS)";
            LOG_ERROR("HttpTransport::sendRequest: Unsupported scheme: " + scheme);
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
        int timeout = (req.timeout >= 0) ? req.timeout : m_timeout;
        int connectTimeout = (req.connect_timeout >= 0) ? req.connect_timeout : m_connectTimeout;
        // 设置 HTTP 请求超时（单位：秒）
        hvReq.timeout = static_cast<uint16_t>(std::min(timeout / 1000, 65535));
        hvReq.connect_timeout = static_cast<uint16_t>(std::min(connectTimeout / 1000, 65535));
        // 发送请求
        HttpResponse hvHttpResponse;
        int ret = m_client->send(&hvReq, &hvHttpResponse);
        
        if (ret != 0) {
            resp.statusCode = -1;
            resp.errorMessage = "Request failed with code: " + std::to_string(ret) 
                              + ", URL: " + req.url;
            LOG_ERROR("HttpTransport::sendRequest: Request failed, ret=" + std::to_string(ret) 
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
            LOG_WARN("HttpTransport::sendRequest: HTTP error " + std::to_string(resp.statusCode) 
                    + ": " + reason + ", URL=" + req.url);
        } else {
            LOG_DEBUG("HttpTransport::sendRequest: Success, status=" + std::to_string(resp.statusCode) 
                     + ", body_size=" + std::to_string(resp.body.size()) + ", URL=" + req.url);
        }
        
        m_connected = true;
        return 0;
        
    } catch (const std::exception& e) {
        resp.statusCode = -1;
        resp.errorMessage = std::string("Exception: ") + e.what();
        LOG_ERROR("HttpTransport::sendRequest exception: " + resp.errorMessage + ", URL=" + req.url);
        m_connected = false;
        return -1;
    } catch (...) {
        resp.statusCode = -1;
        resp.errorMessage = "Unknown exception";
        LOG_ERROR("HttpTransport::sendRequest unknown exception, URL=" + req.url);
        m_connected = false;
        return -1;
    }
}

int HttpTransport::receiveResponse(Response& resp) {
    // libhv 同步 API 在 sendRequest 中已经完成
    // 这里可以返回上次的响应或实现异步接收
    if (m_connected) {
        return 0;
    }
    resp.statusCode = -1;
    resp.errorMessage = "No active connection";
    return -1;
}

bool HttpTransport::connect() {
    // libhv HttpClient 不需要显式连接
    // 在 sendRequest 时自动连接
    return m_connected && m_client != nullptr;
}

void HttpTransport::disconnect() {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_client.reset();
    m_connected = false;
}

bool HttpTransport::isConnected() const {
    return m_connected && m_client != nullptr;
}

std::string HttpTransport::getTransportType() const {
    return "http";
}

void HttpTransport::setTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_timeout = milliseconds;
}

void HttpTransport::setConnectTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_connectTimeout = milliseconds;
}

void HttpTransport::setReadTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_readTimeout = milliseconds;
}

void HttpTransport::setWriteTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_writeTimeout = milliseconds;
}

void HttpTransport::setCloseTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_closeTimeout = milliseconds;
}

void HttpTransport::setKeepaliveTimeout(int milliseconds) {
    boost::lock_guard<boost::mutex> lock(m_clientMutex);
    m_keepaliveTimeout = milliseconds;
}

} // namespace fw
} // namespace intertwine
