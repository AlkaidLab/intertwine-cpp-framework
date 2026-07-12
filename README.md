# AlkaidLab intertwine-cpp-framework

> 瑶光·捕梦网 C++ 框架

[![C++11](https://img.shields.io/badge/C%2B%2B-11-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/11)
[![CMake](https://img.shields.io/badge/CMake-3.14%2B-064F8C?logo=cmake)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-BSD--3--Clause-0B5FFF)](LICENSE)

面向服务端程序的 C++11 基础框架。它在 libhv 之上提供路由、中间件、HTTP/HTTPS/TCP/WebSocket 传输、文件发送、异步任务和常用工具组件。

公开 API 位于 `intertwine::fw`，CMake 包与链接目标为 `intertwine_cpp_framework`。

[中文文档](#文档) · [English documentation](doc_en/README.md)

## 适用场景

| 能力 | 组件 |
| --- | --- |
| HTTP 服务 | `Application`、`Router`、`Context`、`MiddlewareChain` |
| 客户端通信 | `ITransport`、`TransportFactory` |
| 文件发送 | `IFileTransfer`、`FileTransferFactory` |
| 并发控制 | `SupervisedThreadPool`、`LockfreeQueue`、`SPSCQueue`、`FlowController` |
| 基础工具 | 配置、日志、JWT、密码哈希、证书、JSON、ID 和时间处理 |

## 快速开始

```bash
git clone --recursive https://github.com/AlkaidLab/intertwine-cpp-framework.git
cd intertwine-cpp-framework
./build.sh --test
```

Windows：

```powershell
git clone --recursive https://github.com/AlkaidLab/intertwine-cpp-framework.git
Set-Location intertwine-cpp-framework
.\build.ps1 -Test
```

构建脚本会准备 vcpkg 与 libhv 依赖。可按需要指定 vcpkg 或安装目录：

```bash
./build.sh --vcpkg-root ../vcpkg
./build.sh --install-dir ./out/install
```

## 使用方式

```cpp
#include <intertwine/fw/Application.hpp>
#include <intertwine/fw/Context.hpp>
#include <intertwine/fw/Router.hpp>

namespace fw = intertwine::fw;

int main() {
    fw::Application app;
    fw::Router router;

    router.get("/hello", [](fw::Context& ctx) {
        ctx.json(200, "{\"message\":\"hello\"}");
    });

    app.mount(router);
    app.setPort(8080);
    return app.start();
}
```

### CMake 集成

安装框架后，在项目中查找并链接 `intertwine_cpp_framework`：

```cmake
find_package(intertwine_cpp_framework REQUIRED)
target_link_libraries(your_target PRIVATE intertwine_cpp_framework)
```

若安装位置不在 CMake 的默认搜索路径中，配置项目时传入该前缀：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/framework-install
```

### 作为子模块

```bash
git submodule add \
  https://github.com/AlkaidLab/intertwine-cpp-framework.git \
  third_party/intertwine-cpp-framework
git submodule update --init --recursive
```

## 文档

中文（默认）：

- [API 参考](doc/API.md)
- [架构说明](doc/Architecture.md)
- [模块参考](doc/Modules.md)

English:

- [API Reference](doc_en/API.md)
- [Architecture](doc_en/Architecture.md)
- [Module Guide](doc_en/Modules.md)

## 依赖

- [libhv](https://github.com/ithewei/libhv)
- [Boost](https://www.boost.org/)
- [OpenSSL](https://www.openssl.org/)
- [spdlog](https://github.com/gabime/spdlog)
- [nlohmann/json](https://github.com/nlohmann/json)

依赖由 vcpkg 构建流程解析；测试使用 GTest。

## 许可证

[BSD 3-Clause License](LICENSE)
