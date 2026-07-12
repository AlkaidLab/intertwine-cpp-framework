# Intertwine C++ Framework 模块参考

核心服务 API 见 [API.md](API.md)，依赖与运行模型见 [Architecture.md](Architecture.md)。所有框架类型位于 `intertwine::fw`。

模块不依赖特定上层应用或部署环境，可由任意 C++ 消费方按自身运行模型集成。

## 客户端传输

### TransportTypes

`Request` 包含 URL、方法、header 文本、body 以及总超时、连接、读取、写入、关闭和 keep-alive 超时。

`Response` 包含状态码、header 文本、body 和错误消息；`isSuccess()` 判断状态码是否属于 2xx。

`SslConfig` 提供 CA、客户端证书、私钥、校验策略、TLS 版本和会话复用配置。

### ITransport

| 方法 | 说明 |
|------|------|
| `connect()` | 建立或准备连接 |
| `disconnect()` | 关闭连接 |
| `isConnected()` | 返回连接状态 |
| `sendRequest(req, resp)` | 发送请求并接收当前响应 |
| `receiveResponse(resp)` | 接收后续响应或消息 |
| `getTransportType()` | 返回传输类型文本 |

### TransportFactory

| 工厂方法 | 实现 |
|----------|------|
| `createHttp()` | `HttpTransport` |
| `createHttps(config)` | `HttpsTransport` |
| `createTcp()` | `TcpTransport` |
| `createWebSocket(config)` | `WebSocketTransport` |
| `create(type)` | 按类型文本选择实现；未知值返回空指针 |

HTTP 和 HTTPS 传输支持总超时及分阶段超时。TCP 传输使用数据包队列，WebSocket 传输使用消息队列；两者均提供连接状态同步。

## 服务端传输

### ServerTransport

服务端抽象包含 `start()`、`stop()`、`setRouter()`、`connectionCount()` 和 `type()`。

### HvServerTransport

基于 libhv 实现 HTTP/1.1 与 HTTPS 服务端，支持：

- host 与端口配置；
- Router 绑定；
- 证书与私钥配置；
- 静态目录；
- 活跃连接统计；
- 高级场景下访问底层 `HttpServer` 和 `HttpService`。

## 文件传输

### TransferParams

| 字段 | 说明 |
|------|------|
| `physicalPath` | 文件系统路径，由调用方完成授权和校验 |
| `displayName` | 响应文件名和 MIME 推导依据 |
| `inlineDisposition` | 预览或附件下载 |
| `fileSize` | 文件总大小 |
| `rangeStart` / `rangeEnd` | Range 边界；负值表示未指定 |
| `onComplete` | 传输完成回调 |

### TransferStats

线程安全统计累计传输次数、字节数、活跃传输数、错误数和累计耗时。

### FileTransferFactory

| mode | 策略 | 说明 |
|------|------|------|
| `legacy` | `LegacyTransfer` | 兼容同步响应；按文件大小选择响应体或 writer 分块发送 |
| `stream` | `StreamTransfer` | 在 writer 所属 IO loop 中事件驱动发送 |
| `auto` 或空值 | `StreamTransfer` | 当前默认策略 |
| `accel` | `AccelTransfer` | 设置内部重定向响应头，由前置代理发送文件 |

策略实例各自维护 `TransferStats`。未知 mode 返回空指针。

## 无锁并发

### LockfreeQueue\<T\>

基于 `boost::lockfree::queue` 的 MPMC 队列。

| 方法 | 说明 |
|------|------|
| `LockfreeQueue(capacity)` | 创建固定容量队列 |
| `push(item)` | 入队，队列满时返回 false |
| `pop(item)` | 出队，队列空时返回 false |
| `empty()` / `size()` / `capacity()` | 查询队列状态 |
| `clear()` | 清空队列 |

### SPSCQueue\<T\>

单生产者单消费者环形队列。容量会向上对齐为 2 的幂，接口与 `LockfreeQueue` 一致。

### AtomicCounter

| 方法 | 说明 |
|------|------|
| `add(delta)` / `sub(delta)` | 原子增减并返回新值 |
| `get()` / `set(value)` | 读取或设置值 |
| `reset()` | 归零 |
| `++` / `--` | 前置与后置原子操作 |

### FlowController

| 方法 | 说明 |
|------|------|
| `canPush(size)` | 判断当前队列是否允许继续入队 |
| `tryPush(size)` | 判断并记录一次入队尝试 |
| `recordDrop()` | 记录丢弃 |
| `getDropCount()` / `resetDropCount()` | 查询或重置丢弃计数 |
| `setMaxQueueSize(size)` | 调整队列上限 |
| `setDropOnFull(enabled)` | 调整满队列策略 |

## 线程池

### SupervisedThreadPool

受监督线程池由 worker、任务队列和 supervisor 组成。worker 异常退出后，supervisor 回收旧线程并创建替代线程。

| 方法 | 说明 |
|------|------|
| `getInstance()` | 获取应用级单例 |
| `start(workerCount)` | 启动 worker 和 supervisor |
| `setMaxQueueSize(size)` | 设置任务队列上限；0 表示不限制 |
| `submit(task)` | 非阻塞提交任务 |
| `stop()` | 停止并等待所有线程 |
| `workerCount()` | 返回配置的 worker 数量 |
| `queueSize()` | 返回当前任务队列深度 |
| `isRunning()` | 判断线程池是否运行 |

任务类型为 `boost::function<void()>`。

## 配置与日志

### IniConfig

提供文件或内存 INI 加载、保存、基础类型读写、section 与 key 枚举。底层解析器通过 opaque pointer 隐藏。

### Logger

基于 spdlog 的控制台和滚动文件日志封装：

- `setLevel()` / `getLevel()`；
- `setLogFile()`；
- `setConsoleEnabled()`；
- `trace/debug/info/warn/error()`；
- 对应的 printf 风格方法；
- `flush()`。

全局宏为 `LOG_TRACE`、`LOG_DEBUG`、`LOG_INFO`、`LOG_WARN` 和 `LOG_ERROR`。

### LogConfig

封装 libhv 日志文件、大小、保留天数和级别配置。`initialize()` 提供一次性初始化，`ensureDirectory()` 提供递归目录创建。

## 安全与身份

### PasswordUtil

使用 PBKDF2-HMAC-SHA256、随机盐和迭代次数生成密码哈希。提供 `hash()`、`verify()` 和 `isHashed()`。

### JwtUtil

提供 token 签发、校验、Bearer 提取、时间字段读取和撤销列表管理。撤销列表支持容量限制、清理、持久化回调和恢复加载。

### CertUtil

支持生成自签名证书及读取证书主题、域名、有效期和自签名状态。

### PathGuard

对输入路径进行规范化，拒绝文件系统根目录、受保护系统目录和过浅的绝对路径。调用方仍需结合自己的授权规则进行二次校验。

## 数据与通用工具

### Base64

支持字节指针、字符串和字节向量编码，以及字符指针或字符串解码。

### HashUtil

提供文件 MD5、SHA-256 计算及期望摘要校验。

### JsonUtil

提供容错解析、字符串/整数/布尔字段读取和 JSON 字符串转义。接口同时接受 `nlohmann::json` 与序列化文本。

### IdUtil

提供 UUID v4、带时间前缀的 UUID、Snowflake 数字与字符串 ID。`isSafeId()` 仅接受非空、长度不超过 64 的字母、数字和连字符。

### TimeUtil

提供当前毫秒时间、ISO 8601 格式化、系统时区文本、时间点差值和毫秒字符串转换。

## 使用约束

- 工厂方法可能返回空指针，调用方必须检查。
- 文件传输前应由业务层完成路径授权和范围校验。
- `SPSCQueue` 只能用于单生产者单消费者模型。
- `SupervisedThreadPool::setMaxQueueSize()` 应在 `start()` 前调用。
- TLS 校验选项应由部署环境明确配置，不应在公共示例中写入真实证书位置或服务地址。
