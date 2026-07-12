# Intertwine C++ Framework API 参考

公开 API 位于 `intertwine::fw` 命名空间，CMake 包名与库目标为 `intertwine_cpp_framework`。

本文聚焦核心框架和传输接口。并发与工具类的详细说明见 [Modules.md](Modules.md)。

## Context

`Context` 封装服务端请求、响应和异步 writer。常规 handler 通过 `Context&` 访问数据，无需直接操作 libhv 请求对象。

### 请求读取

| 方法 | 返回 | 说明 |
|------|------|------|
| `method()` | `const char*` | HTTP 方法名称 |
| `methodEnum()` | `int` | 与 libhv 对齐的方法枚举值 |
| `path()` | `const std::string&` | 请求路径 |
| `fullPath()` | `std::string` | 包含查询参数的完整路径 |
| `param(key)` | `std::string` | 路径或查询参数 |
| `header(key)` | `std::string` | 请求头 |
| `eraseRequestHeader(key)` | `void` | 删除请求头 |
| `body()` | `const std::string&` | 请求体 |
| `contentLength()` | `int64_t` | 请求声明的内容长度 |
| `formData(key)` | `std::string` | Multipart 表单字段 |
| `cookie(name)` | `const std::string&` | 请求 Cookie 值 |
| `clientIp()` | `std::string` | Socket 对端地址文本 |
| `clientPort()` | `int` | Socket 对端端口 |
| `httpVersion()` | `int` | `major * 10 + minor` 形式的 HTTP 版本 |

### 响应设置

| 方法 | 说明 |
|------|------|
| `setStatus(code)` / `status()` | 设置或读取状态码 |
| `setHeader(key, value)` | 设置响应头；值中的 CR/LF 会被移除 |
| `responseHeader(key)` | 读取响应头 |
| `removeHeader(key)` | 删除响应头 |
| `setBody(body)` | 设置响应体 |
| `responseBodySize()` | 获取当前响应体字节数 |
| `setContentType(type)` | 设置 Content-Type |
| `setContentTypeByFilename(name)` | 根据文件名推导 Content-Type |
| `serveFile(path)` | 将文件交给同步响应路径处理 |
| `json(status, body)` | 设置 JSON 状态、类型和响应体 |
| `error(status, code, message)` | 生成统一 JSON 错误响应 |
| `setCookie(...)` | 设置响应 Cookie，支持 HttpOnly、SameSite 和 Secure |

### 流式响应

这些方法仅在异步 `ctx_handler` 场景存在 writer 时有效：

| 方法 | 说明 |
|------|------|
| `hasWriter()` | 判断 writer 是否可用 |
| `writeStatus(code)` | 写入状态行 |
| `writeHeader(key, value)` | 写入响应头 |
| `endHeaders(key, length)` | 结束响应头并声明内容长度 |
| `writeBody(data, length)` | 写入一个响应体分片 |
| `writerConnected()` | 判断连接是否仍可写 |
| `writeBufsize()` | 获取待发送缓冲区大小 |
| `end()` | 结束响应 |
| `writerOwnership()` | 获取保持 writer 存活的共享所有权句柄 |
| `markStreamingHandoff()` | 将响应生命周期交给流式策略 |
| `isStreamingHandoff()` | 判断是否已完成流式接管 |

### 上下文 KV

| 方法 | 说明 |
|------|------|
| `set(key, value)` | 写入中间件上下文值 |
| `get(key, defaultValue)` | 读取上下文值 |
| `has(key)` | 判断键是否存在 |

### 测试构造器

`TestContextBuilder` 用于在测试中构造 `Context`，支持设置 method、path、header、client address 文本和 body。底层 libhv 对象由构造器内部管理。

## MiddlewareChain

中间件签名为：

```cpp
using Next = std::function<int()>;
using MiddlewareFn = std::function<int(Context&, Next)>;
```

调用 `next()` 进入下一层；不调用则终止当前链。`next()` 返回内层 handler 或中间件的状态码。

| 方法 | 说明 |
|------|------|
| `use(fn)` | 添加匿名中间件 |
| `use(name, fn)` | 添加命名中间件 |
| `execute(ctx, finalHandler)` | 执行完整洋葱链 |
| `size()` | 返回中间件数量 |
| `clear()` | 清空中间件 |

## Router

Handler 签名为 `std::function<void(Context&)>`。

| 方法 | 说明 |
|------|------|
| `get/post/put/del/patch(path, handler)` | 注册同步路由 |
| `getAsync(path, handler)` | 注册异步 GET 路由 |
| `use(fn)` / `use(name, fn)` | 添加路由级中间件 |
| `setAsyncDispatcher(fn)` | 设置异步任务派发器；未设置时同步执行 |
| `setAsyncTaskTracker(fn)` | 跟踪在途异步任务数量 |
| `setPostprocessor(fn)` | 设置请求后处理器 |
| `setErrorHandler(fn)` | 设置未匹配路由处理器 |
| `bind(service)` | 将路由绑定到 libhv `HttpService` |
| `routeCount()` | 返回已注册路由数量 |

## Application

`Application` 管理 HTTP 服务生命周期并封装常用 libhv 服务配置。

### 回调与路由

| 方法 | 说明 |
|------|------|
| `setHeaderHandler(fn)` | 请求头解析后的预处理回调 |
| `use(fn)` | 添加服务级中间件 |
| `setErrorHandler(fn)` | 设置未匹配路由处理 |
| `setPostprocessor(fn)` | 设置请求完成后的处理器 |
| `mount(router)` | 挂载 Router |

服务级回调签名为 `std::function<int(Context&)>`。

### 静态内容与代理

| 方法 | 说明 |
|------|------|
| `setDocumentRoot(path)` | 设置静态文件根目录 |
| `mountStatic(prefix, directory)` | 将 URL 前缀映射到静态目录 |
| `proxy(prefix, target)` | 配置反向代理映射 |
| `setProxyTimeout(connect, read, write)` | 设置代理超时，单位毫秒 |

### 服务配置与生命周期

| 方法 | 说明 |
|------|------|
| `setHost(host)` | 设置监听主机文本 |
| `setPort(port)` | 设置 HTTP 端口 |
| `setHttpsPort(port)` | 设置 HTTPS 端口 |
| `setWorkerThreads(count)` | 设置 IO worker 数量 |
| `setKeepaliveTimeout(ms)` | 设置 keep-alive 超时 |
| `setLimitRate(kbps)` | 设置发送速率限制；负值表示不限速 |
| `setServerName(names)` | 设置允许的 Host 名称列表 |
| `serverNames()` | 返回规范化后的 Host 名称集合 |
| `isAllowedHost(host)` | 判断 Host 是否允许 |
| `enableSsl(cert, key)` | 使用证书和私钥启用 HTTPS |
| `start()` | 非阻塞启动服务；返回 0 表示成功 |
| `stop()` | 停止服务 |
| `connectionNum()` | 返回活跃连接数 |
| `workerThreadsCount()` | 返回 worker 数量 |
| `makeAsyncDispatcher()` | 创建基于 libhv async 的任务派发器 |
| `cleanupAsync()` | 清理全局异步执行器 |

## ServerTransport

`ServerTransport` 是服务端传输接口：

| 方法 | 说明 |
|------|------|
| `start()` / `stop()` | 启动或停止服务 |
| `setRouter(router)` | 设置 Router |
| `connectionCount()` | 获取活跃连接数 |
| `type()` | 返回协议类型文本 |

`HvServerTransport` 提供基于 libhv 的 HTTP/HTTPS 实现，并支持端口、证书、静态目录及高级 `HttpServer`/`HttpService` 配置。

## 客户端传输

`ITransport` 统一 HTTP、HTTPS、TCP 和 WebSocket 客户端调用：

| 方法 | 说明 |
|------|------|
| `connect()` / `disconnect()` | 管理连接 |
| `isConnected()` | 判断连接状态 |
| `sendRequest(request, response)` | 发送请求 |
| `receiveResponse(response)` | 接收后续响应或消息 |
| `getTransportType()` | 返回传输类型 |

`TransportFactory::create(type)` 支持 `http`、`https`、`tcp`、`websocket`、`ws` 和 `wss`。具体参数结构见 `TransportTypes.hpp`。

## 文件传输

`IFileTransfer::send(Context&, TransferParams)` 定义服务端文件发送策略。

`TransferParams` 包含文件标识、展示名、处置方式、大小、Range 边界和完成回调。`TransferStats` 提供传输次数、字节数、活跃数、错误数和累计耗时统计。

`FileTransferFactory::create(mode, accelPrefix)` 支持：

| mode | 行为 |
|------|------|
| `legacy` | 小文件进入响应体，大文件使用兼容流式路径 |
| `stream` | 使用 writer 事件驱动分块发送 |
| `auto` 或空值 | 当前选择 `stream` |
| `accel` | 设置内部重定向响应头，由前置代理发送文件 |

未知 mode 返回空指针。

## HttpConstants

`HttpStatus` 的数值遵循 libhv，核心状态通过 `static_assert` 进行编译期校验。常量包含 `Next`、`Close`、常用 2xx/4xx/5xx 状态以及 Range 响应相关状态。

`HttpMethod` 提供 DELETE、GET、HEAD、POST、PUT、CONNECT、OPTIONS、TRACE 和 PATCH 的数值常量。

## 设计约束

1. 公共 handler 使用 `Context&`，业务层不直接持有 libhv 请求或响应指针。
2. 核心接口保持 C++11 兼容。
3. 中间件通过 Context KV 传递内部数据，不需要构造内部 HTTP header。
4. 异步流式策略必须显式接管并结束响应生命周期。
5. 传输工厂对未知类型返回空指针，调用方必须检查结果。
