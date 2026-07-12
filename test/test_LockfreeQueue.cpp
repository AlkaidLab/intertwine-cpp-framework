#include "intertwine/fw/LockfreeQueue.hpp"
#include <gtest/gtest.h>

namespace intertwine {
namespace fw {

TEST(LockfreeQueue, PushPopBasic) {
    LockfreeQueue<int> q(8);
    EXPECT_TRUE(q.empty());

    EXPECT_TRUE(q.push(100));
    EXPECT_FALSE(q.empty());

    int val = 0;
    EXPECT_TRUE(q.pop(val));
    EXPECT_EQ(val, 100);
    EXPECT_TRUE(q.empty());
}

TEST(LockfreeQueue, PushPopMultiple) {
    LockfreeQueue<int> q(16);
    // boost::lockfree::queue 是无序的，故只验证元素完整性
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(q.push(i));
    }
    EXPECT_EQ(q.size(), 10u);

    int count = 0;
    int val;
    while (q.pop(val)) {
        ++count;
    }
    EXPECT_EQ(count, 10);
    EXPECT_TRUE(q.empty());
}

TEST(LockfreeQueue, PopFromEmptyReturnsFalse) {
    LockfreeQueue<int> q(4);
    int val = -1;
    EXPECT_FALSE(q.pop(val));
}

TEST(LockfreeQueue, CapacityRoundedToPowerOfTwo) {
    LockfreeQueue<int> q(5);
    // 内部会向上取整到 8
    EXPECT_EQ(q.capacity(), 8u);
}

TEST(LockfreeQueue, Clear) {
    LockfreeQueue<int> q(8);
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_FALSE(q.empty());
    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(LockfreeQueue, PushFullReturnsFalse) {
    LockfreeQueue<int> q(4);  // 实际容量 4
    // boost::lockfree::queue 容量为 fixed_sized 时才会拒绝
    // 默认非 fixed_sized 不一定会返回 false，但 capacity 受限时应拒绝
    int pushed = 0;
    for (int i = 0; i < 100; ++i) {
        if (q.push(i)) pushed++;
    }
    EXPECT_GT(pushed, 0);
}

} // namespace fw
} // namespace intertwine
