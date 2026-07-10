#ifndef ALKAIDLAB_FW_CONTEXT_HPP
#define ALKAIDLAB_FW_CONTEXT_HPP

// Server-side request/response context abstraction.
// Wraps libhv HttpRequest/HttpResponse behind pimpl, so handlers interact only
// through Context& without depending on libhv headers at all.
// Also provides a KV store to replace the X-Internal-* header pattern.

#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace alkaidlab {
namespace fw {

class Context {
  public:
    /** Sync handler scenario (req/resp raw pointers, cast internally) */
    Context(void* req, void* resp);

    /** ctx_handler / streaming handler scenario (shared_ptr<HttpContext>, cast internally) */
    explicit Context(const void* httpCtxSharedPtr);

    ~Context();

    /* -- Request access -- */

    /** HTTP method string ("GET", "POST", ...) */
    const char* method() const;

    /** Request path (e.g. "/api/fs/list") */
    const std::string& path() const;

    /** URL path/query parameter (libhv stores both uniformly) */
    std::string param(const char* key) const;

    /** Request header */
    std::string header(const char* key) const;

    /** Request body */
    const std::string& body() const;

    /** Client IP (socket peer) */
    std::string clientIp() const;

    /** Client port */
    int clientPort() const;

    /** Multipart form field value by name */
    std::string formData(const char* key) const;

    /* -- Response -- */

    void setStatus(int code);
    int status() const;

    void setHeader(const char* key, const std::string& val);
    std::string responseHeader(const char* key) const;

    void setBody(const std::string& body);
    void setContentType(const char* ct);
    void setContentTypeByFilename(const char* filename);

    /** Read file content into response body */
    void serveFile(const char* filepath);

    /** Convenience: set JSON response (Content-Type + status + body) */
    void json(int statusCode, const std::string& jsonBody);

    /** Convenience: JSON error response {"error":"<code>","message":"<detail>"} */
    void error(int statusCode, const char* code, const char* message = nullptr);

    /* -- Streaming (ctx_handler scenario) -- */

    bool hasWriter() const;
    void writeStatus(int code);
    void writeHeader(const char* key, const char* val);
    int endHeaders(const char* key, int64_t contentLength);
    int writeBody(const char* data, int len);
    bool writerConnected() const;
    /** Current write buffer size (bytes pending in kernel + libhv buffer) */
    size_t writeBufsize() const;
    int end();

    /* -- Context KV store (middleware -> handler data passing) -- */

    void set(const std::string& key, const std::string& val);
    std::string get(const std::string& key, const std::string& def = "") const;
    bool has(const std::string& key) const;

    /* -- Request metadata -- */

    /** Request content length (body size declared by client) */
    int64_t contentLength() const;

    /** Full request path including query string (e.g. "/api?key=val") */
    std::string fullPath() const;

    /* -- Response metadata -- */

    /** Response body size (bytes written so far) */
    size_t responseBodySize() const;

    /** Remove a response header (e.g. strip internal headers before sending) */
    void removeHeader(const char* key);

    /** Erase a request header (e.g. strip spoofed internal headers) */
    void eraseRequestHeader(const char* key);

    /* -- Cookie -- */

    /** Read a request cookie value by name (empty if not found). */
    const std::string& cookie(const char* name) const;

    /** Set a response cookie with common options.
     *  @param sameSite  0=None, 1=Lax, 2=Strict
     *  @param secure    Restrict the cookie to HTTPS requests */
    void setCookie(const char* name, const char* value,
                   const char* path, int maxAge,
                   bool httpOnly, int sameSite);
    void setCookie(const char* name, const char* value,
                   const char* path, int maxAge,
                   bool httpOnly, int sameSite, bool secure);

    /* -- HTTP version -- */

    /** HTTP version code: major*10 + minor (e.g. 11 = HTTP/1.1, 20 = HTTP/2) */
    int httpVersion() const;

    /** HTTP method enum value (libhv http_method) */
    int methodEnum() const;

    /* -- Writer ownership (for async transfer strategies) -- */

    /** Acquire shared ownership of the underlying writer.
     *  The returned handle keeps the writer alive after Context is destroyed.
     *  Returns empty shared_ptr if no writer is available (sync handler). */
    std::shared_ptr<void> writerOwnership() const;

    /** Mark this async response as taken over by a streaming handler.
     *  When set, the async dispatcher MUST NOT call writer->End() after the
     *  handler returns — the streaming code (e.g. StreamTransfer) is responsible
     *  for calling End() when all body data has been pumped out.
     *
     *  Without this flag, calling End() prematurely will close the socket when
     *  the client used Connection: close, causing later EPOLLOUT-driven writes
     *  to fail with isConnected()=false. */
    void markStreamingHandoff();

    /** Whether the handler has handed off response lifecycle to a streaming writer. */
    bool isStreamingHandoff() const;

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

  public:
    /** Move constructor (pimpl transfer). */
    Context(Context&& other);

  private:    struct Impl;
    Impl* m_impl;

    /** Private: used by TestContextBuilder (owns internal hv objects) */
    explicit Context(Impl* impl);

    friend class TestContextBuilder;
};

/* ── Test support ── */

/**
 * Builder for mock Context objects in unit tests.
 * All hv object creation is hidden inside Context.cpp — test code
 * never needs to #include <hv/...>.
 *
 * Usage:
 *     auto ctx = TestContextBuilder()
 *         .method("GET").path("/api/test")
 *         .clientIp("1.2.3.4")
 *         .header("X-Forwarded-For", "10.0.0.1")
 *         .build();
 */
class TestContextBuilder {
  public:
    TestContextBuilder();
    ~TestContextBuilder();

    TestContextBuilder& method(const char* m);
    TestContextBuilder& path(const char* p);
    TestContextBuilder& header(const char* key, const char* val);
    TestContextBuilder& clientIp(const char* ip);
    TestContextBuilder& body(const std::string& b);

    Context build();

  private:
    TestContextBuilder(const TestContextBuilder&);
    TestContextBuilder& operator=(const TestContextBuilder&);

    struct BuilderImpl;
    BuilderImpl* m_impl;
};

} // namespace fw
} // namespace alkaidlab

#endif // ALKAIDLAB_FW_CONTEXT_HPP
