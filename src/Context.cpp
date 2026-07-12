#include "intertwine/fw/Context.hpp"
#include <hv/HttpMessage.h>
#include <hv/HttpContext.h>
#include <hv/HttpResponseWriter.h>
#include <unordered_map>

namespace intertwine {
namespace fw {

struct Context::Impl {
    HttpRequest* req;
    HttpResponse* resp;
    HttpContextPtr httpCtx; // null for sync handlers
    std::unordered_map<std::string, std::string> data;
    bool streamingHandoff;

    /* Owned objects for test builder — keeps req/resp alive */
    HttpRequest*  ownedReq;
    HttpResponse* ownedResp;

    Impl(HttpRequest* r, HttpResponse* s)
        : req(r), resp(s), streamingHandoff(false), ownedReq(0), ownedResp(0) {}
    Impl(const HttpContextPtr& ctx)
        : req(ctx->request.get()), resp(ctx->response.get()), httpCtx(ctx),
          streamingHandoff(false), ownedReq(0), ownedResp(0) {}

    ~Impl() {
        delete ownedReq;
        delete ownedResp;
    }
};

/* -- Constructors -- */

Context::Context(void* req, void* resp)
    : m_impl(new Impl(static_cast<HttpRequest*>(req),
                       static_cast<HttpResponse*>(resp))) {
}

Context::Context(const void* httpCtxSharedPtr)
    : m_impl(new Impl(*static_cast<const HttpContextPtr*>(httpCtxSharedPtr))) {
}

Context::~Context() {
    delete m_impl;
}

Context::Context(Context&& other) : m_impl(other.m_impl) {
    other.m_impl = 0;
}

/* -- Request access -- */

const char* Context::method() const {
    return http_method_str(m_impl->req->method);
}

const std::string& Context::path() const {
    return m_impl->req->path;
}

std::string Context::param(const char* key) const {
    return m_impl->req->GetParam(key);
}

std::string Context::header(const char* key) const {
    return m_impl->req->GetHeader(key);
}

const std::string& Context::body() const {
    return m_impl->req->body;
}

std::string Context::clientIp() const {
    return m_impl->req->client_addr.ip;
}

int Context::clientPort() const {
    return m_impl->req->client_addr.port;
}

std::string Context::formData(const char* key) const {
    return m_impl->req->GetFormData(key);
}

/* -- Response -- */

void Context::setStatus(int code) {
    m_impl->resp->status_code = static_cast<http_status>(code);
}

int Context::status() const {
    return m_impl->resp->status_code;
}

void Context::setHeader(const char* key, const std::string& val) {
    // Strip CR/LF to prevent HTTP header injection (CWE-113)
    bool hasCrlf = false;
    for (std::string::size_type i = 0; i < val.size(); ++i) {
        if (val[i] == '\r' || val[i] == '\n') { hasCrlf = true; break; }
    }
    if (hasCrlf) {
        std::string safe;
        safe.reserve(val.size());
        for (std::string::size_type i = 0; i < val.size(); ++i) {
            if (val[i] != '\r' && val[i] != '\n')
                safe += val[i];
        }
        m_impl->resp->SetHeader(key, safe);
    } else {
        m_impl->resp->SetHeader(key, val);
    }
}

std::string Context::responseHeader(const char* key) const {
    return m_impl->resp->GetHeader(key);
}

void Context::setBody(const std::string& body) {
    m_impl->resp->body = body;
}

void Context::setContentType(const char* ct) {
    m_impl->resp->SetContentType(ct);
}

void Context::setContentTypeByFilename(const char* filename) {
    m_impl->resp->SetContentTypeByFilename(filename);
}

void Context::serveFile(const char* filepath) {
    // Reject path traversal sequences (CWE-22)
    if (filepath) {
        for (const char* p = filepath; *p; ++p) {
            if (p[0] == '.' && p[1] == '.') {
                m_impl->resp->status_code = HTTP_STATUS_FORBIDDEN;
                m_impl->resp->body = "{\"error\":\"Forbidden\"}";
                return;
            }
        }
    }
    m_impl->resp->File(filepath);
}

void Context::json(int statusCode, const std::string& jsonBody) {
    m_impl->resp->status_code = static_cast<http_status>(statusCode);
    m_impl->resp->content_type = APPLICATION_JSON;
    m_impl->resp->SetHeader("Content-Type", "application/json; charset=utf-8");
    m_impl->resp->body = jsonBody;
}

void Context::error(int statusCode, const char* code, const char* message) {
    m_impl->resp->status_code = static_cast<http_status>(statusCode);
    m_impl->resp->SetHeader("Content-Type", "application/json; charset=utf-8");
    if (message && *message) {
        std::string body = "{\"error\":\"";
        body += code;
        body += "\",\"message\":\"";
        for (const char* p = message; *p; ++p) {
            if (*p == '"' || *p == '\\')
                body += '\\';
            body += *p;
        }
        body += "\"}";
        m_impl->resp->body = body;
    } else {
        m_impl->resp->body = std::string("{\"error\":\"") + code + "\"}";
    }
}

/* -- Streaming -- */

bool Context::hasWriter() const {
    return m_impl->httpCtx && m_impl->httpCtx->writer;
}

void Context::writeStatus(int code) {
    if (hasWriter())
        m_impl->httpCtx->writer->WriteStatus(static_cast<http_status>(code));
}

void Context::writeHeader(const char* key, const char* val) {
    if (!hasWriter()) return;
    // Strip CR/LF to prevent HTTP header injection (CWE-113)
    bool hasCrlf = false;
    for (const char* p = val; *p; ++p) {
        if (*p == '\r' || *p == '\n') { hasCrlf = true; break; }
    }
    if (hasCrlf) {
        std::string safe;
        for (const char* p = val; *p; ++p) {
            if (*p != '\r' && *p != '\n')
                safe += *p;
        }
        m_impl->httpCtx->writer->WriteHeader(key, safe.c_str());
    } else {
        m_impl->httpCtx->writer->WriteHeader(key, val);
    }
}

int Context::endHeaders(const char* key, int64_t contentLength) {
    if (hasWriter())
        return m_impl->httpCtx->writer->EndHeaders(key, contentLength);
    return -1;
}

int Context::writeBody(const char* data, int len) {
    if (hasWriter())
        return m_impl->httpCtx->writer->WriteBody(data, len);
    return -1;
}

bool Context::writerConnected() const {
    return hasWriter() && m_impl->httpCtx->writer->isConnected();
}

size_t Context::writeBufsize() const {
    if (hasWriter())
        return m_impl->httpCtx->writer->writeBufsize();
    return 0;
}

int Context::end() {
    if (hasWriter())
        return m_impl->httpCtx->writer->End();
    return -1;
}

/* -- Context KV -- */

void Context::set(const std::string& key, const std::string& val) {
    m_impl->data[key] = val;
}

std::string Context::get(const std::string& key, const std::string& def) const {
    std::unordered_map<std::string, std::string>::const_iterator it = m_impl->data.find(key);
    return it != m_impl->data.end() ? it->second : def;
}

bool Context::has(const std::string& key) const {
    return m_impl->data.find(key) != m_impl->data.end();
}

/* -- Request metadata -- */

int64_t Context::contentLength() const {
    return m_impl->req->content_length;
}

std::string Context::fullPath() const {
    std::string result = m_impl->req->path;
    if (result.find('?') == std::string::npos && !m_impl->req->query_params.empty()) {
        result += "?";
        bool first = true;
        for (std::map<std::string, std::string>::const_iterator it = m_impl->req->query_params.begin();
             it != m_impl->req->query_params.end(); ++it) {
            if (!first)
                result += "&";
            result += it->first + "=" + it->second;
            first = false;
        }
    }
    return result;
}

/* -- Response metadata -- */

size_t Context::responseBodySize() const {
    return m_impl->resp->body.size();
}

void Context::removeHeader(const char* key) {
    m_impl->resp->headers.erase(key);
}

void Context::eraseRequestHeader(const char* key) {
    m_impl->req->headers.erase(key);
}

/* -- Cookie -- */

const std::string& Context::cookie(const char* name) const {
    return m_impl->req->GetCookie(name).value;
}

void Context::setCookie(const char* name, const char* value,
                        const char* path, int maxAge,
                        bool httpOnly, int sameSite) {
    setCookie(name, value, path, maxAge, httpOnly, sameSite, false);
}

void Context::setCookie(const char* name, const char* value,
                        const char* path, int maxAge,
                        bool httpOnly, int sameSite, bool secure) {
    HttpCookie ck;
    ck.name = name;
    ck.value = value;
    ck.path = path;
    ck.max_age = maxAge;
    ck.httponly = httpOnly;
    ck.secure = secure;
    switch (sameSite) {
    case 0:
        ck.samesite = HttpCookie::None;
        break;
    case 1:
        ck.samesite = HttpCookie::Lax;
        break;
    case 2:
        ck.samesite = HttpCookie::Strict;
        break;
    default:
        ck.samesite = HttpCookie::Lax;
        break;
    }
    m_impl->resp->AddCookie(ck);
}

/* -- HTTP version -- */

int Context::httpVersion() const {
    return m_impl->req->http_major * 10 + m_impl->req->http_minor;
}

int Context::methodEnum() const {
    return static_cast<int>(m_impl->req->method);
}

/* -- Writer ownership -- */

std::shared_ptr<void> Context::writerOwnership() const {
    if (hasWriter())
        return m_impl->httpCtx->writer;
    return std::shared_ptr<void>();
}

void Context::markStreamingHandoff() {
    m_impl->streamingHandoff = true;
}

bool Context::isStreamingHandoff() const {
    return m_impl->streamingHandoff;
}

/* ── Private constructor for TestContextBuilder ── */

Context::Context(Impl* impl) : m_impl(impl) {}

/* ── TestContextBuilder ── */

struct TestContextBuilder::BuilderImpl {
    HttpRequest  req;
    HttpResponse resp;

    BuilderImpl() {
        req.method = HTTP_GET;
        req.path = "/";
    }
};

TestContextBuilder::TestContextBuilder() : m_impl(new BuilderImpl) {}
TestContextBuilder::~TestContextBuilder() { delete m_impl; }

TestContextBuilder& TestContextBuilder::method(const char* m) {
    m_impl->req.method = http_method_enum(m);
    return *this;
}

TestContextBuilder& TestContextBuilder::path(const char* p) {
    m_impl->req.path = p;
    return *this;
}

TestContextBuilder& TestContextBuilder::header(const char* key, const char* val) {
    m_impl->req.SetHeader(key, val);
    return *this;
}

TestContextBuilder& TestContextBuilder::clientIp(const char* ip) {
    m_impl->req.client_addr.ip = ip;
    return *this;
}

TestContextBuilder& TestContextBuilder::body(const std::string& b) {
    m_impl->req.body = b;
    return *this;
}

Context TestContextBuilder::build() {
    // Create owned copies; Impl destructor will free them.
    HttpRequest*  ownedReq  = new HttpRequest(m_impl->req);
    HttpResponse* ownedResp = new HttpResponse(m_impl->resp);
    Context::Impl* impl = new Context::Impl(ownedReq, ownedResp);
    impl->ownedReq  = ownedReq;
    impl->ownedResp = ownedResp;
    return Context(impl);
}

} // namespace fw
} // namespace intertwine
