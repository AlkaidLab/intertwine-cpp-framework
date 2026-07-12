# AlkaidLab intertwine-cpp-framework

> The Intertwine C++ framework

[![C++11](https://img.shields.io/badge/C%2B%2B-11-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/11)
[![CMake](https://img.shields.io/badge/CMake-3.14%2B-064F8C?logo=cmake)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-BSD--3--Clause-0B5FFF)](../LICENSE)

`intertwine-cpp-framework` is a C++11 foundation library for server-side applications. Built on libhv, it provides routing, middleware, HTTP/HTTPS/TCP/WebSocket transport, file delivery, asynchronous work, and common utility components.

The public API is `intertwine::fw`. The exported CMake package and target are both named `intertwine_cpp_framework`.

[中文主页](../README.md) · [English documentation](#documentation)

## What it provides

| Area | Components |
| --- | --- |
| HTTP services | `Application`, `Router`, `Context`, `MiddlewareChain` |
| Client transport | `ITransport`, `TransportFactory` |
| File delivery | `IFileTransfer`, `FileTransferFactory` |
| Concurrency | `SupervisedThreadPool`, `LockfreeQueue`, `SPSCQueue`, `FlowController` |
| Utilities | Configuration, logging, JWT, password hashing, certificates, JSON, IDs, and time handling |

## Build from source

```bash
git clone --recursive https://github.com/AlkaidLab/intertwine-cpp-framework.git
cd intertwine-cpp-framework
./build.sh --test
```

On Windows:

```powershell
git clone --recursive https://github.com/AlkaidLab/intertwine-cpp-framework.git
Set-Location intertwine-cpp-framework
.\build.ps1 -Test
```

The build scripts prepare vcpkg and libhv when required. To reuse a vcpkg checkout or choose an installation prefix:

```bash
./build.sh --vcpkg-root ../vcpkg
./build.sh --install-dir ./out/install
```

## Minimal example

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

## CMake integration

After installing the framework, find and link `intertwine_cpp_framework` from your project:

```cmake
find_package(intertwine_cpp_framework REQUIRED)
target_link_libraries(your_target PRIVATE intertwine_cpp_framework)
```

If the installation prefix is outside CMake's default search paths, pass it while configuring your project:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/framework-install
```

## Use as a submodule

```bash
git submodule add \
  https://github.com/AlkaidLab/intertwine-cpp-framework.git \
  third_party/intertwine-cpp-framework
git submodule update --init --recursive
```

## Documentation

- [API Reference](API.md)
- [Architecture](Architecture.md)
- [Module Guide](Modules.md)

The default Chinese documentation is available in [`../doc/`](../doc/).

## Dependencies

- [libhv](https://github.com/ithewei/libhv)
- [Boost](https://www.boost.org/)
- [OpenSSL](https://www.openssl.org/)
- [spdlog](https://github.com/gabime/spdlog)
- [nlohmann/json](https://github.com/nlohmann/json)

Dependencies are resolved by the vcpkg build flow. Tests use GTest.

## License

[BSD 3-Clause License](../LICENSE)
