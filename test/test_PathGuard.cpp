/**
 * @file test_PathGuard.cpp
 * @brief fw::PathGuard 单元测试
 */
#include "intertwine/fw/PathGuard.hpp"
#include <gtest/gtest.h>

using intertwine::fw::PathGuard;

// ---- 危险路径 ----

TEST(PathGuard, RejectsEmpty) {
    std::string r;
    EXPECT_FALSE(PathGuard::isSafe("", r));
    EXPECT_NE(r.find("empty"), std::string::npos);
}

TEST(PathGuard, RejectsRoot) {
    std::string r;
    EXPECT_FALSE(PathGuard::isSafe("/", r));
    EXPECT_FALSE(PathGuard::isSafe("///", r));
}

TEST(PathGuard, RejectsRootHome) {
    std::string r;
    EXPECT_FALSE(PathGuard::isSafe("/root", r));
    EXPECT_FALSE(PathGuard::isSafe("/root/", r));
}

TEST(PathGuard, RejectsEtc) {
    std::string r;
    EXPECT_FALSE(PathGuard::isSafe("/etc", r));
    EXPECT_FALSE(PathGuard::isSafe("/etc/", r));
}

TEST(PathGuard, RejectsSystemDirs) {
    std::string r;
    EXPECT_FALSE(PathGuard::isSafe("/bin", r));
    EXPECT_FALSE(PathGuard::isSafe("/sbin", r));
    EXPECT_FALSE(PathGuard::isSafe("/usr", r));
    EXPECT_FALSE(PathGuard::isSafe("/usr/bin", r));
    EXPECT_FALSE(PathGuard::isSafe("/lib", r));
    EXPECT_FALSE(PathGuard::isSafe("/dev", r));
    EXPECT_FALSE(PathGuard::isSafe("/proc", r));
    EXPECT_FALSE(PathGuard::isSafe("/sys", r));
    EXPECT_FALSE(PathGuard::isSafe("/boot", r));
    EXPECT_FALSE(PathGuard::isSafe("/var", r));
    EXPECT_FALSE(PathGuard::isSafe("/var/log", r));
    EXPECT_FALSE(PathGuard::isSafe("/home", r));
}

TEST(PathGuard, RejectsShallowAbsolute) {
    std::string r;
    // /tmp, /opt 等一级目录也被拒绝（深度 < 2）
    EXPECT_FALSE(PathGuard::isSafe("/tmp", r));
    EXPECT_FALSE(PathGuard::isSafe("/opt", r));
}

// ---- 安全路径 ----

TEST(PathGuard, AcceptsDeepAbsolute) {
    std::string r;
    EXPECT_TRUE(PathGuard::isSafe("/opt/intertwine", r));
    EXPECT_TRUE(PathGuard::isSafe("/opt/intertwine/data", r));
    EXPECT_TRUE(PathGuard::isSafe("/home/user/uploads", r));
    EXPECT_TRUE(PathGuard::isSafe("/tmp/intertwine_data", r));
    EXPECT_TRUE(PathGuard::isSafe("/var/intertwine", r));
}

TEST(PathGuard, AcceptsRelativePath) {
    std::string r;
    // 相对路径会拼接 cwd，深度通常足够
    EXPECT_TRUE(PathGuard::isSafe("data/uploads", r));
    EXPECT_TRUE(PathGuard::isSafe("data/upload_tmp", r));
    EXPECT_TRUE(PathGuard::isSafe("log", r));
}

TEST(PathGuard, AcceptsCurrentDir) {
    std::string r;
    EXPECT_TRUE(PathGuard::isSafe("./data", r));
}

// ---- 简化版 ----

TEST(PathGuard, SimplifiedOverload) {
    EXPECT_FALSE(PathGuard::isSafe("/"));
    EXPECT_FALSE(PathGuard::isSafe("/root"));
    EXPECT_TRUE(PathGuard::isSafe("data/uploads"));
}

// ---- 末尾斜杠归一化 ----

TEST(PathGuard, TrailingSlashNormalized) {
    std::string r;
    EXPECT_FALSE(PathGuard::isSafe("/root///", r));
    EXPECT_FALSE(PathGuard::isSafe("/etc///", r));
}
