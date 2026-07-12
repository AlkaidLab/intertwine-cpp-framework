# intertwine_cpp_framework — Lightweight C++ HTTP framework abstraction layer
# Wraps libhv into Context / Router / Middleware / ServerTransport.
#
# 独立构建（预编译安装模式）：
#   cmake -B build -DLIBHV_INCLUDE_DIR=... -DCMAKE_INSTALL_PREFIX=../intertwine_cpp_framework_install
#   cmake --build build
#   cmake --install build

cmake_minimum_required(VERSION 3.10)

# 使用 vcpkg 的 Boost 时避免 CMP0167 警告（FindBoost 仍由 vcpkg wrapper 使用）
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(intertwine_cpp_framework VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FW_SOURCES
    src/Context.cpp
    src/Middleware.cpp
    src/Router.cpp
    src/HvTransport.cpp
    src/IniConfig.cpp
    src/Base64.cpp
    src/LogConfig.cpp
    src/Application.cpp
    src/HttpConstants.cpp
    src/HttpTransport.cpp
    src/HttpsTransport.cpp
    src/TcpTransport.cpp
    src/WebSocketTransport.cpp
    src/TransportFactory.cpp
    src/HashUtil.cpp
    src/TimeUtil.cpp
    src/PasswordUtil.cpp
    src/CertUtil.cpp
    src/IdUtil.cpp
    src/JwtUtil.cpp
    src/Logger.cpp
    src/AtomicCounter.cpp
    src/FlowController.cpp
    src/SupervisedThreadPool.cpp
    src/LegacyTransfer.cpp
    src/AccelTransfer.cpp
    src/StreamTransfer.cpp
    src/FileTransferFactory.cpp
)

add_library(intertwine_cpp_framework STATIC ${FW_SOURCES})

set_target_properties(intertwine_cpp_framework PROPERTIES
    OUTPUT_NAME "intertwine_cpp_framework"
    POSITION_INDEPENDENT_CODE ON
)

# Windows 宏污染防御: 仅保留 LogLevel::ERROR / std::min / std::max 等标识符
#   NOMINMAX            — 禁用 windows.h 的 min/max 宏
#   WIN32_LEAN_AND_MEAN — 米 windows.h 只包含核心部分
#   NOGDI               — 禁用 wingdi.h 里的 ERROR 等绘图宏
# (DELETE 来自 winnt.h 不可避免, 需要源码层面用不同名字)
if(WIN32)
    target_compile_definitions(intertwine_cpp_framework PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        NOGDI
    )
    # 强制注入 POSIX 兼容垫片（localtime_r / gmtime_r 等）。
    # PUBLIC 选项会传递给所有链接 intertwine_cpp_framework 的目标。
    target_compile_options(intertwine_cpp_framework PUBLIC -include intertwine/fw/WinPosixShim.hpp)
endif()

# 公开头文件目录：使用方 #include "intertwine/fw/Context.hpp"
target_include_directories(intertwine_cpp_framework PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# libhv（优先使用外部传入，否则从 build_cache 获取）
if(NOT LIBHV_INCLUDE_DIR)
    set(LIBHV_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build_cache/libhv_install/include")
endif()
if(NOT LIBHV_LIB)
    foreach(_candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/build_cache/libhv_install/lib/libhv_static.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/build_cache/libhv_install/lib/libhv.a")
        if(EXISTS "${_candidate}")
            set(LIBHV_LIB "${_candidate}")
            break()
        endif()
    endforeach()
endif()
# 调用方可传入相对路径；在生成安装配置前统一解析，避免后续工作目录影响它。
if(LIBHV_INCLUDE_DIR AND NOT IS_ABSOLUTE "${LIBHV_INCLUDE_DIR}")
    get_filename_component(LIBHV_INCLUDE_DIR "${LIBHV_INCLUDE_DIR}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(LIBHV_LIB AND NOT TARGET "${LIBHV_LIB}" AND NOT IS_ABSOLUTE "${LIBHV_LIB}")
    get_filename_component(LIBHV_LIB "${LIBHV_LIB}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(LIBHV_INCLUDE_DIR)
    target_include_directories(intertwine_cpp_framework PRIVATE ${LIBHV_INCLUDE_DIR})
endif()
if(LIBHV_LIB)
    target_link_libraries(intertwine_cpp_framework PRIVATE ${LIBHV_LIB})
endif()

# pthread（transport 模块使用 boost::mutex / boost::condition_variable）
find_package(Threads REQUIRED)
target_link_libraries(intertwine_cpp_framework PRIVATE Threads::Threads)

# Boost（transport / TimeUtil / IdUtil / PathGuard 使用）
if(NOT Boost_FOUND)
    find_package(Boost REQUIRED COMPONENTS thread chrono system filesystem random)
endif()
target_include_directories(intertwine_cpp_framework PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(intertwine_cpp_framework PRIVATE
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_RANDOM_LIBRARY}
)
if(TARGET Boost::uuid)
    target_link_libraries(intertwine_cpp_framework PRIVATE Boost::uuid)
endif()

# OpenSSL（HvTransport HTTPS + HashUtil / CertUtil / JwtUtil / PasswordUtil）
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    target_compile_definitions(intertwine_cpp_framework PRIVATE HAVE_OPENSSL_CRYPTO)
    target_include_directories(intertwine_cpp_framework PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(intertwine_cpp_framework PRIVATE ${OPENSSL_LIBRARIES})
endif()

# spdlog（Logger 使用）
find_package(spdlog CONFIG QUIET)
find_package(fmt CONFIG QUIET)
if(spdlog_FOUND)
    set(SPDLOG_TARGET spdlog::spdlog_header_only)
    if(fmt_FOUND)
        set_target_properties(spdlog::spdlog_header_only PROPERTIES
            INTERFACE_LINK_LIBRARIES "Threads::Threads;fmt::fmt-header-only"
        )
    endif()
    target_link_libraries(intertwine_cpp_framework PUBLIC ${SPDLOG_TARGET})
endif()

# nlohmann/json（JsonUtil 使用，header-only）
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(intertwine_cpp_framework PUBLIC nlohmann_json::nlohmann_json)
endif()

if(WIN32)
    target_link_libraries(intertwine_cpp_framework PRIVATE iphlpapi ws2_32)
endif()

# ---- 安装规则 ----
install(TARGETS intertwine_cpp_framework
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/intertwine DESTINATION include)

# 公共头文件包含 hv 类型，安装包必须同时提供其头文件。
if(LIBHV_INCLUDE_DIR AND EXISTS "${LIBHV_INCLUDE_DIR}/hv")
    install(DIRECTORY "${LIBHV_INCLUDE_DIR}/hv" DESTINATION include)
endif()
# libhv.a 也安装进 install prefix，确保 cmake config 使用相对路径后仍可找到
if(LIBHV_LIB AND EXISTS "${LIBHV_LIB}")
    install(FILES "${LIBHV_LIB}" DESTINATION lib RENAME libhv.a)
endif()

# 生成 cmake config：消费方 find_package(intertwine_cpp_framework) 即可获得完整依赖
# 记录构建时的 libhv 路径和依赖信息
set(_FW_LIBHV_LIB_PATH "")
if(LIBHV_LIB)
    # LIBHV_LIB 可能是路径或目标名，如果是路径则直接使用
    if(EXISTS "${LIBHV_LIB}")
        set(_FW_LIBHV_LIB_PATH "${LIBHV_LIB}")
    else()
        # 可能是 cmake target，尝试获取其 IMPORTED_LOCATION
        get_target_property(_loc ${LIBHV_LIB} IMPORTED_LOCATION)
        if(_loc)
            set(_FW_LIBHV_LIB_PATH "${_loc}")
        endif()
    endif()
endif()
set(_FW_LIBHV_INCLUDE "${LIBHV_INCLUDE_DIR}")
set(_FW_OPENSSL_FOUND ${OPENSSL_FOUND})
set(_FW_HAS_IO_URING FALSE)
if(DEFINED _LIBHV_HAS_IO_URING AND _LIBHV_HAS_IO_URING)
    set(_FW_HAS_IO_URING TRUE)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/intertwine_cpp_framework-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/intertwine_cpp_framework-config.cmake"
    @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/intertwine_cpp_framework-config.cmake"
    DESTINATION lib/cmake/intertwine_cpp_framework)


# ---- 单元测试（构建时自动运行） ----
option(FW_BUILD_TESTS "Build and run intertwine_cpp_framework unit tests" ON)
if(FW_BUILD_TESTS)
    find_package(GTest CONFIG QUIET)
    if(GTest_FOUND)
        set(GTEST_FOUND TRUE)
        set(GTEST_LIBRARIES GTest::gtest)
        set(GTEST_MAIN_LIBRARIES GTest::gtest_main)
        set(GTEST_INCLUDE_DIRS "")
    endif()
    if(GTEST_FOUND)
        enable_testing()
        set(FW_TEST_SOURCES
            test/test_Context.cpp
            test/test_Middleware.cpp
            test/test_Router.cpp
            test/test_Base64.cpp
            test/test_LogConfig.cpp
            test/test_IniConfig.cpp
            test/test_HttpConstants.cpp
            test/test_Application.cpp
            test/test_AtomicCounter.cpp
            test/test_FlowController.cpp
            test/test_SPSCQueue.cpp
            test/test_LockfreeQueue.cpp
            test/test_Base64Util.cpp
            test/test_HashUtil.cpp
            test/test_IdUtil.cpp
            test/test_JsonUtil.cpp
            test/test_JwtUtil.cpp
            test/test_PasswordUtil.cpp
            test/test_PathGuard.cpp
            test/test_TimeUtil.cpp
        )
        foreach(TEST_SRC ${FW_TEST_SOURCES})
            get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
            add_executable(${TEST_NAME} ${TEST_SRC})
            if(NOT "${GTEST_INCLUDE_DIRS}" STREQUAL "")
                target_include_directories(${TEST_NAME} PRIVATE ${GTEST_INCLUDE_DIRS})
            endif()
            target_link_libraries(${TEST_NAME} intertwine_cpp_framework ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)
            if(LIBHV_INCLUDE_DIR)
                target_include_directories(${TEST_NAME} PRIVATE ${LIBHV_INCLUDE_DIR})
            endif()
            if(LIBHV_LIB)
                target_link_libraries(${TEST_NAME} ${LIBHV_LIB})
            endif()
            if(OPENSSL_FOUND)
                target_link_libraries(${TEST_NAME} ${OPENSSL_LIBRARIES})
            endif()
            add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
        endforeach()
        message(STATUS "intertwine_cpp_framework: unit tests enabled")
    else()
        message(STATUS "intertwine_cpp_framework: GTest not found, tests skipped")
    endif()
endif()
