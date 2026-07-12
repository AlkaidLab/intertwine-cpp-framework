#include "intertwine/fw/SPSCQueue.hpp"
#include <gtest/gtest.h>
#include <string>

namespace intertwine {
namespace fw {

TEST(SPSCQueue, PushPopBasic) {
    SPSCQueue<int> q(8);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    EXPECT_TRUE(q.push(42));
    EXPECT_FALSE(q.empty());

    int val = 0;
    EXPECT_TRUE(q.pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueue, PushPopMultiple) {
    SPSCQueue<int> q(16);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(q.push(i * 10));
    }
    EXPECT_EQ(q.size(), 10u);

    for (int i = 0; i < 10; ++i) {
        int val = -1;
        EXPECT_TRUE(q.pop(val));
        EXPECT_EQ(val, i * 10);
    }
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueue, PopFromEmptyReturnsFalse) {
    SPSCQueue<int> q(4);
    int val = -1;
    EXPECT_FALSE(q.pop(val));
}

TEST(SPSCQueue, PushFullReturnsFalse) {
    // 容量取 2 的幂：实际可用 capacity-1 个槽位（环形缓冲区保留一个空位）
    SPSCQueue<int> q(4);  // => capacity=4, 可用=3
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));
    EXPECT_FALSE(q.push(4));  // 满
}

TEST(SPSCQueue, CapacityRoundedToPowerOfTwo) {
    SPSCQueue<int> q(5);
    // 5 向上取 -> 8
    EXPECT_EQ(q.capacity(), 8u);
}

TEST(SPSCQueue, MoveSemantics) {
    SPSCQueue<std::string> q(8);
    std::string s = "hello";
    EXPECT_TRUE(q.push(std::move(s)));

    std::string out;
    EXPECT_TRUE(q.pop(out));
    EXPECT_EQ(out, "hello");
}

TEST(SPSCQueue, Clear) {
    SPSCQueue<int> q(8);
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_FALSE(q.empty());
    q.clear();
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueue, WrapAround) {
    // 测试环形缓冲区回绕
    SPSCQueue<int> q(4);  // 可用 3 个槽
    for (int round = 0; round < 5; ++round) {
        EXPECT_TRUE(q.push(round * 10 + 1));
        EXPECT_TRUE(q.push(round * 10 + 2));
        int v;
        EXPECT_TRUE(q.pop(v));
        EXPECT_EQ(v, round * 10 + 1);
        EXPECT_TRUE(q.pop(v));
        EXPECT_EQ(v, round * 10 + 2);
    }
}

} // namespace fw
} // namespace intertwine
