# Intertwine C++ Framework API Reference

The public API is in the `intertwine::fw` namespace. The CMake package and target name is `intertwine_cpp_framework`.

This document covers framework contracts only. Application entry points, deployment, and service management are defined by each consumer.

For concurrency and utility components, see [Modules.md](Modules.md).

## Context

`Context` wraps a server request, response, and asynchronous writer. Handlers use `Context&` without directly manipulating libhv request objects.

### Request access

| Method | Return type | Purpose |
|---|---|---|
| `method()` | `const char*` | HTTP method text |
| `methodEnum()` | `int` | libhv-compatible method value |
| `path()` | `const std::string&` | Request path |
| `fullPath()` | `std::string` | Path including query parameters |
| `param(key)` | `std::string` | Route or query parameter |
| `header(key)` | `std::string` | Request header |
| `eraseRequestHeader(key)` | `void` | Remove a request header |
| `body()` | `const std::string&` | Request body |
| `contentLength()` | `int64_t` | Declared content length |
| `formData(key)` | `std::string` | Multipart form value |
| `cookie(name)` | `const std::string&` | Request cookie |
| `clientIp()` / `clientPort()` | `std::string` / `int` | Peer address and port |
| `httpVersion()` | `int` | HTTP version as `major * 10 + minor` |

### Response construction

| Method | Purpose |
|---|---|
| `setStatus(code)` / `status()` | Set or read the status code |
| `setHeader(key, value)` | Set a response header; CR/LF is removed from values |
| `responseHeader(key)` / `removeHeader(key)` | Read or remove a response header |
| `setBody(body)` / `responseBodySize()` | Set the body or read its current size |
| `setContentType(type)` / `setContentTypeByFilename(name)` | Set a content type directly or infer it from a filename |
| `serveFile(path)` | Serve a file through the synchronous response path |
| `json(status, body)` / `error(status, code, message)` | Build JSON success or error responses |
| `setCookie(...)` | Set a cookie with HttpOnly, SameSite, and Secure options |

### Streaming responses

The following methods require an asynchronous `ctx_handler` with an available writer.

| Method | Purpose |
|---|---|
| `hasWriter()` / `writerConnected()` | Check writer availability and connection state |
| `writeStatus(code)` / `writeHeader(key, value)` | Write response metadata |
| `endHeaders(key, length)` | Finish headers and declare the body length |
| `writeBody(data, length)` / `writeBufsize()` | Write one chunk or inspect queued bytes |
| `end()` | Finish the response |
| `writerOwnership()` | Obtain shared ownership that keeps the writer alive |
| `markStreamingHandoff()` / `isStreamingHandoff()` | Transfer and inspect streaming lifecycle ownership |

### Context key-value storage

`set(key, value)`, `get(key, defaultValue)`, and `has(key)` carry internal middleware state without exposing it through HTTP headers.

`TestContextBuilder` constructs test contexts with a method, path, headers, peer address text, and body while owning the underlying libhv objects.

## MiddlewareChain

```cpp
using Next = std::function<int()>;
using MiddlewareFn = std::function<int(Context&, Next)>;
```

Calling `next()` enters the next layer. Omitting it terminates the chain. The return value is the status from the inner middleware or final handler.

| Method | Purpose |
|---|---|
| `use(fn)` / `use(name, fn)` | Add anonymous or named middleware |
| `execute(ctx, finalHandler)` | Run the complete onion chain |
| `size()` / `clear()` | Inspect or clear registered middleware |

## Router

Handlers use `std::function<void(Context&)>`.

| Method | Purpose |
|---|---|
| `get/post/put/del/patch(path, handler)` | Register synchronous routes |
| `getAsync(path, handler)` | Register an asynchronous GET route |
| `use(...)` | Add route-level middleware |
| `setAsyncDispatcher(fn)` / `setAsyncTaskTracker(fn)` | Configure asynchronous dispatch and in-flight tracking |
| `setPostprocessor(fn)` / `setErrorHandler(fn)` | Configure request completion and unmatched-route handling |
| `bind(service)` / `routeCount()` | Bind to a libhv `HttpService` or inspect route count |

## Application

`Application` manages the HTTP service lifecycle and common libhv configuration.

| Area | Methods |
|---|---|
| Callbacks and routing | `setHeaderHandler`, `use`, `setErrorHandler`, `setPostprocessor`, `mount` |
| Static content and proxying | `setDocumentRoot`, `mountStatic`, `proxy`, `setProxyTimeout` |
| Listening and lifecycle | `setHost`, `setPort`, `setHttpsPort`, `setWorkerThreads`, `start`, `stop` |
| Runtime controls | `setKeepaliveTimeout`, `setLimitRate`, `setServerName`, `serverNames`, `isAllowedHost` |
| TLS and async support | `enableSsl`, `connectionNum`, `workerThreadsCount`, `makeAsyncDispatcher`, `cleanupAsync` |

Service-level callbacks use `std::function<int(Context&)>`. `start()` is non-blocking and returns zero on success.

## Server and client transport

`ServerTransport` exposes `start()`, `stop()`, `setRouter()`, `connectionCount()`, and `type()`. `HvServerTransport` supplies the libhv HTTP/HTTPS implementation and supports ports, certificates, static directories, and advanced `HttpServer` / `HttpService` configuration.

`ITransport` unifies HTTP, HTTPS, TCP, and WebSocket clients through `connect()`, `disconnect()`, `isConnected()`, `sendRequest()`, `receiveResponse()`, and `getTransportType()`.

`TransportFactory::create(type)` accepts `http`, `https`, `tcp`, `websocket`, `ws`, and `wss`. `TransportTypes.hpp` defines the request, response, and TLS option structures.

## File transfer

`IFileTransfer::send(Context&, TransferParams)` defines server-side file delivery. `TransferParams` carries the physical path, display name, content disposition, file size, Range bounds, and completion callback. `TransferStats` records transfer count, bytes, active transfers, errors, and elapsed time.

`FileTransferFactory::create(mode, accelPrefix)` supports:

| Mode | Behavior |
|---|---|
| `legacy` | Small files use the response body; larger files use the compatibility streaming path |
| `stream` | Uses writer-driven chunked delivery |
| `auto` or empty | Selects `stream` |
| `accel` | Sets an internal redirect header for a capable reverse proxy |

Unknown modes return a null pointer.

## HTTP constants and constraints

`HttpStatus` follows libhv values and validates key values with `static_assert`. `HttpMethod` provides numeric constants for the supported methods.

1. Public handlers use `Context&`; business code does not retain libhv request or response pointers.
2. Public interfaces remain C++11-compatible.
3. Middleware uses Context key-value storage for internal state.
4. Asynchronous streaming must explicitly take ownership and finish the response lifecycle.
5. Transport factories may return null; callers must check the result.
