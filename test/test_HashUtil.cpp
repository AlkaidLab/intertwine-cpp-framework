#include "intertwine/fw/HashUtil.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstdio>
#include <boost/filesystem.hpp>

namespace intertwine {
namespace fw {

static std::string writeTempFile(const std::string& content) {
    boost::filesystem::create_directories("/tmp/intertwine_cpp_framework_tmp");
    boost::filesystem::path model("/tmp/intertwine_cpp_framework_tmp/hash_%%%%-%%%%");
    boost::filesystem::path p = boost::filesystem::unique_path(model);
    std::ofstream ofs(p.string(), std::ios::binary);
    if (!ofs) return "";
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.close();
    return p.string();
}

TEST(HashUtil, FileMD5HexFormat) {
    std::string path = writeTempFile("hello");
    if (path.empty()) { GTEST_SKIP() << "temp file creation failed"; }
    std::string md5 = HashUtil::fileMD5Hex(path);
    unlink(path.c_str());
    EXPECT_EQ(md5.size(), 32u);
    for (char c : md5) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(HashUtil, FileSHA256HexFormat) {
    std::string path = writeTempFile("hello");
    if (path.empty()) { GTEST_SKIP() << "temp file creation failed"; }
    std::string sha = HashUtil::fileSHA256Hex(path);
    unlink(path.c_str());
    EXPECT_EQ(sha.size(), 64u);
    for (char c : sha) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(HashUtil, VerifyCorrect) {
    std::string path = writeTempFile("hello");
    if (path.empty()) { GTEST_SKIP() << "temp file creation failed"; }
    std::string md5 = HashUtil::fileMD5Hex(path);
    std::string sha = HashUtil::fileSHA256Hex(path);
    EXPECT_TRUE(HashUtil::verify(path, md5, sha));
    unlink(path.c_str());
}

TEST(HashUtil, VerifyWrongMD5) {
    std::string path = writeTempFile("hello");
    if (path.empty()) { GTEST_SKIP() << "temp file creation failed"; }
    std::string sha = HashUtil::fileSHA256Hex(path);
    EXPECT_FALSE(HashUtil::verify(path, "00000000000000000000000000000000", sha));
    unlink(path.c_str());
}

} // namespace fw
} // namespace intertwine
