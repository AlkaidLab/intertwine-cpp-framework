# AlkaidLab intertwine-cpp-framework

瑶光·捕梦网 cpp框架

Intertwine 系列的 C++11 基础框架，提供服务端路由与中间件、HTTP/HTTPS/TCP/WebSocket 传输、异步任务、文件传输及常用基础组件。

公开 CMake 包名与库目标为 `intertwine_cpp_framework`，C++ API 位于 `intertwine::fw`。

本仓库独立发布和使用；文档不约定任何上层应用的可执行文件名、服务名、部署路径或运行配置。

## 特性

- 最低兼容 C++11，面向需要兼容较旧编译器和运行环境的项目。
- 使用 `Application`、`Router`、`Context` 和 `MiddlewareChain` 组织 HTTP 服务。
- 提供 HTTP、HTTPS、TCP 和 WebSocket 客户端传输抽象。
- 提供 legacy、stream 和 accel 三种服务端文件传输策略。
- 提供受监督线程池、无锁队列、流量控制及原子计数器。
- 提供配置、日志、密码哈希、JWT、证书、ID、时间和 JSON 等工具。
- 通过 pimpl 和适配层限制业务代码对 libhv 的直接依赖。

## 快速开始

```bash
git clone --recursive https://github.com/AlkaidLab/intertwine-cpp-framework.git
cd intertwine-cpp-framework
./build.sh --test
```

Windows PowerShell：

```powershell
git clone --recursive https://github.com/AlkaidLab/intertwine-cpp-framework.git
Set-Location intertwine-cpp-framework
.\build.ps1 -Test
```

构建脚本会准备 libhv 和 vcpkg 依赖，并将框架安装到构建缓存目录。可通过参数覆盖默认位置：

```bash
./build.sh --vcpkg-root ../vcpkg
./build.sh --install-dir ./out/install
./build.sh --clean --test
```

## C++11 示例

```cpp
#include "intertwine/fw/Application.hpp"
#include "intertwine/fw/Context.hpp"
#include "intertwine/fw/HttpConstants.hpp"
#include "intertwine/fw/Middleware.hpp"
#include "intertwine/fw/Router.hpp"

#include <iostream>

namespace fw = intertwine::fw;

int main() {
    fw::Application app;
    fw::Router router;

    router.use("request-log", [](fw::Context&, fw::Next next) -> int {
        return next();
    });

    router.get("/hello", [](fw::Context& ctx) {
        ctx.json(fw::HttpStatus::Ok, "{\"message\":\"hello\"}");
    });

    app.mount(router);
    app.setPort(8080);
    app.setWorkerThreads(4);

    if (app.start() != 0) {
        return 1;
    }

    std::cout << "Press Enter to stop." << std::endl;
    std::cin.get();
    app.stop();
    return 0;
}
```

## 作为子模块集成

```bash
git submodule add \
  https://github.com/AlkaidLab/intertwine-cpp-framework.git \
  third_party/intertwine-cpp-framework
git submodule update --init --recursive

bash third_party/intertwine-cpp-framework/build.sh \
  --vcpkg-root ./vcpkg \
  --install-dir ./out/intertwine-cpp-framework
```

消费方按以下包名链接：

```cmake
find_package(intertwine_cpp_framework REQUIRED)
target_link_libraries(your_target PRIVATE intertwine_cpp_framework)
```

## 模块

| 分类 | 主要组件 | 用途 |
|------|----------|------|
| 核心框架 | `Application`、`Router`、`Context`、`MiddlewareChain` | 服务生命周期、路由、中间件和请求响应封装 |
| 服务端传输 | `ServerTransport`、`HvServerTransport` | 基于 libhv 的 HTTP/HTTPS 服务端适配 |
| 客户端传输 | `ITransport`、`TransportFactory` | HTTP、HTTPS、TCP 和 WebSocket 请求传输 |
| 文件传输 | `IFileTransfer`、`FileTransferFactory` | 内存响应、事件驱动流式发送和代理加速 |
| 并发 | `SupervisedThreadPool`、`LockfreeQueue`、`SPSCQueue` | 异步任务、无锁队列和流量控制 |
| 工具 | `IniConfig`、`Logger`、`JwtUtil`、`PasswordUtil` 等 | 配置、安全、日志、序列化和通用能力 |

完整说明：

- [API 参考](doc/API.md)
- [架构说明](doc/Architecture.md)
- [模块参考](doc/Modules.md)

## 构建要求

- C++11 编译器
- CMake 3.14 或更高版本
- Git（用于初始化子模块）

Boost、OpenSSL、spdlog、fmt、nlohmann/json 和 GTest 等依赖由构建流程通过 vcpkg 解析。

## 测试

CMake 当前注册 20 个测试可执行程序，覆盖核心框架、配置与安全工具、日志和并发容器。

```bash
./build.sh --test
```

## 依赖项目

- [libhv](https://github.com/ithewei/libhv) — HTTP 服务端和网络事件基础设施
- [Boost](https://www.boost.org/) — 线程、文件系统、UUID、原子操作和无锁容器
- [OpenSSL](https://www.openssl.org/) — TLS、证书、哈希和密码学能力
- [spdlog](https://github.com/gabime/spdlog) — 日志实现
- [nlohmann/json](https://github.com/nlohmann/json) — JSON 支持
- [fmt](https://github.com/fmtlib/fmt) — 格式化支持

## 许可证

[BSD 3-Clause License](LICENSE)
