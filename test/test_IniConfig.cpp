#include "intertwine/fw/IniConfig.hpp"
#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>

namespace intertwine {
namespace fw {

class IniConfigTest : public ::testing::Test {
protected:
    static const char* kTmpFile;

    void TearDown() override {
        std::remove(kTmpFile);
    }

    void writeFile(const char* content) {
        std::ofstream f(kTmpFile);
        f << content;
    }
};
const char* IniConfigTest::kTmpFile = "/tmp/fw_test_ini.ini";

TEST_F(IniConfigTest, LoadFromMem) {
    IniConfig cfg;
    ASSERT_TRUE(cfg.loadFromMem(
        "[server]\n"
        "port = 8080\n"
        "host = 0.0.0.0\n"
    ));
    EXPECT_EQ(cfg.getString("port", "server"), "8080");
    EXPECT_EQ(cfg.getInt("port", "server"), 8080);
    EXPECT_EQ(cfg.getString("host", "server"), "0.0.0.0");
}

TEST_F(IniConfigTest, LoadFromFile) {
    writeFile(
        "[app]\n"
        "debug = true\n"
        "rate = 3.14\n"
    );
    IniConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(kTmpFile));
    EXPECT_TRUE(cfg.getBool("debug", "app"));
    EXPECT_NEAR(cfg.getFloat("rate", "app"), 3.14f, 0.01f);
}

TEST_F(IniConfigTest, DefaultValues) {
    IniConfig cfg;
    cfg.loadFromMem("");
    EXPECT_EQ(cfg.getString("missing"), "");
    EXPECT_EQ(cfg.getInt("missing", "", 42), 42);
    EXPECT_FALSE(cfg.getBool("missing", "", false));
    EXPECT_NEAR(cfg.getFloat("missing", "", 1.5f), 1.5f, 0.01f);
}

TEST_F(IniConfigTest, SetAndGet) {
    IniConfig cfg;
    cfg.loadFromMem("");
    cfg.setString("key", "value", "sec");
    EXPECT_EQ(cfg.getString("key", "sec"), "value");

    cfg.setInt("num", 123, "sec");
    EXPECT_EQ(cfg.getInt("num", "sec"), 123);

    cfg.setBool("flag", true, "sec");
    EXPECT_TRUE(cfg.getBool("flag", "sec"));

    cfg.setFloat("ratio", 2.718f, "sec");
    EXPECT_NEAR(cfg.getFloat("ratio", "sec"), 2.718f, 0.01f);
}

TEST_F(IniConfigTest, SaveAs) {
    IniConfig cfg;
    cfg.loadFromMem("[test]\nk1 = v1\n");
    cfg.setString("k2", "v2", "test");
    ASSERT_TRUE(cfg.saveAs(kTmpFile));

    // 重新读取验证
    IniConfig cfg2;
    ASSERT_TRUE(cfg2.loadFromFile(kTmpFile));
    EXPECT_EQ(cfg2.getString("k1", "test"), "v1");
    EXPECT_EQ(cfg2.getString("k2", "test"), "v2");
}

TEST_F(IniConfigTest, Sections) {
    IniConfig cfg;
    cfg.loadFromMem("[a]\nx=1\n[b]\ny=2\n");
    std::vector<std::string> secs = cfg.sections();
    EXPECT_GE(secs.size(), 2u);
}

TEST_F(IniConfigTest, Unload) {
    IniConfig cfg;
    cfg.loadFromMem("[s]\nk=v\n");
    EXPECT_EQ(cfg.getString("k", "s"), "v");
    cfg.unload();
    EXPECT_EQ(cfg.getString("k", "s"), "");
}

} // namespace fw
} // namespace intertwine
