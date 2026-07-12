#include "intertwine/fw/FlowController.hpp"
#include <gtest/gtest.h>
#include <cstdint>

namespace intertwine {
namespace fw {

TEST(FlowController, CanPushUnderLimit) {
    FlowController fc(100);
    EXPECT_TRUE(fc.canPush(0));
    EXPECT_TRUE(fc.canPush(50));
    EXPECT_TRUE(fc.canPush(99));
}

TEST(FlowController, CanPushAtLimit) {
    FlowController fc(100);
    EXPECT_FALSE(fc.canPush(100));
    EXPECT_FALSE(fc.canPush(200));
}

TEST(FlowController, TryPushSuccess) {
    FlowController fc(10);
    EXPECT_TRUE(fc.tryPush(5));
    EXPECT_EQ(fc.getDropCount(), 0u);
}

TEST(FlowController, TryPushFullDropMode) {
    FlowController fc(10, true);  // drop on full
    EXPECT_FALSE(fc.tryPush(10));
    EXPECT_EQ(fc.getDropCount(), 1u);
}

TEST(FlowController, TryPushFullBlockMode) {
    FlowController fc(10, false);  // block mode
    EXPECT_FALSE(fc.tryPush(10));
    // 阻塞模式也返回 false，但不记录 drop
    EXPECT_EQ(fc.getDropCount(), 0u);
}

TEST(FlowController, RecordDrop) {
    FlowController fc(10);
    fc.recordDrop();
    fc.recordDrop();
    fc.recordDrop();
    EXPECT_EQ(fc.getDropCount(), 3u);
}

TEST(FlowController, ResetDropCount) {
    FlowController fc(10);
    fc.recordDrop();
    fc.recordDrop();
    EXPECT_EQ(fc.getDropCount(), 2u);
    fc.resetDropCount();
    EXPECT_EQ(fc.getDropCount(), 0u);
}

TEST(FlowController, GetSetMaxQueueSize) {
    FlowController fc(100);
    EXPECT_EQ(fc.getMaxQueueSize(), 100u);
    fc.setMaxQueueSize(512);
    EXPECT_EQ(fc.getMaxQueueSize(), 512u);
    // 新限制生效
    EXPECT_TRUE(fc.canPush(500));
    EXPECT_FALSE(fc.canPush(512));
}

TEST(FlowController, GetSetDropOnFull) {
    FlowController fc(10, true);
    EXPECT_TRUE(fc.isDropOnFull());
    fc.setDropOnFull(false);
    EXPECT_FALSE(fc.isDropOnFull());
}

TEST(FlowController, MultipleDropsAccumulate) {
    FlowController fc(5, true);
    for (int i = 0; i < 100; ++i) {
        fc.tryPush(10);  // 全部被拒
    }
    EXPECT_EQ(fc.getDropCount(), 100u);
}

} // namespace fw
} // namespace intertwine
