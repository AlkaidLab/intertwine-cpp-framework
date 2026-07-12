# Intertwine C++ Framework Module Guide

Core contracts are documented in [API.md](API.md); design boundaries are in [Architecture.md](Architecture.md). All framework types are in `intertwine::fw`.

Modules do not depend on a specific application or deployment environment. Consumers select and integrate them according to their own runtime model.

## Client transport

`Request` contains URL, method, headers, body, and total, connect, read, write, close, and keep-alive timeouts. `Response` contains status, headers, body, and an error message. `SslConfig` supplies CA, client certificate, private key, verification policy, TLS version, and session reuse settings.

`ITransport` defines connection, send, receive, and status operations; see [API.md](API.md#server-and-client-transport) for the contract.

| Factory method | Implementation |
|---|---|
| `createHttp()` | `HttpTransport` |
| `createHttps(config)` | `HttpsTransport` |
| `createTcp()` | `TcpTransport` |
| `createWebSocket(config)` | `WebSocketTransport` |
| `create(type)` | Selects by type text; unknown values return null |

HTTP and HTTPS support total and phase-specific timeouts. TCP uses packet queues, and WebSocket uses message queues.

## Server transport and file delivery

`ServerTransport` decouples routing from the listener implementation. `HvServerTransport` implements libhv HTTP/1.1 and HTTPS, with host and port settings, Router binding, certificates, static directories, connection statistics, and optional access to `HttpServer` and `HttpService`.

`FileTransferFactory` selects a server-side delivery strategy:

| Mode | Strategy | Use |
|---|---|---|
| `legacy` | `LegacyTransfer` | Compatibility path; chooses a response body or writer chunks by file size |
| `stream` | `StreamTransfer` | Event-driven delivery on the writer's IO loop |
| `auto` or empty | `StreamTransfer` | Current default |
| `accel` | `AccelTransfer` | Internal redirect header for a capable reverse proxy |

Strategies maintain separate `TransferStats`. Unknown modes return null. The business layer must authorize paths and validate Range input before calling a strategy.

## Concurrency

`LockfreeQueue<T>` is a fixed-capacity MPMC queue. `SPSCQueue<T>` is a single-producer/single-consumer ring buffer whose capacity is rounded up to a power of two.

`AtomicCounter` provides atomic add, subtract, get, set, reset, and increment/decrement operations.

`FlowController` controls queue admission with `canPush`, `tryPush`, `recordDrop`, drop-count accessors, a queue limit, and a full-queue policy.

`SupervisedThreadPool` combines workers, a task queue, and a supervisor. When a worker exits unexpectedly, the supervisor reclaims it and creates a replacement. Configure `setMaxQueueSize()` before `start()`. Tasks use `boost::function<void()>`.

## Configuration, logging, and security

| Component | Responsibility |
|---|---|
| `IniConfig` | Load, save, enumerate, and read/write basic INI values while hiding the parser behind an opaque pointer |
| `Logger` / `LogConfig` | spdlog-backed console and rolling-file logging; libhv log file and retention configuration |
| `PasswordUtil` | PBKDF2-HMAC-SHA256 hashing, random salts, and verification |
| `JwtUtil` | Token issue and validation, Bearer extraction, time fields, and a bounded revocation list |
| `CertUtil` | Self-signed certificate generation and certificate metadata inspection |
| `PathGuard` | Path normalization and rejection of roots, protected directories, and shallow absolute paths |

`JwtUtil` supports persistence callbacks for its revocation list. `PathGuard` is a safety layer, not a replacement for consumer authorization rules.

## Data and common utilities

| Component | Responsibility |
|---|---|
| `Base64` | Encode and decode pointers, strings, and byte vectors |
| `HashUtil` | MD5 and SHA-256 file calculation and expected-digest verification |
| `JsonUtil` | Tolerant parsing, scalar access, and JSON string escaping |
| `IdUtil` | UUID v4, time-prefixed UUID, Snowflake IDs, and safe-ID validation |
| `TimeUtil` | Current milliseconds, ISO 8601 formatting, timezone text, and duration conversion |

Factory methods may return null and must be checked. TLS verification is a consumer decision; public examples must not embed real certificate locations or service addresses.
