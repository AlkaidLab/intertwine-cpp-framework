#include "intertwine/fw/AtomicCounter.hpp"
#include <gtest/gtest.h>
#include <cstdint>

namespace intertwine {
namespace fw {

TEST(AtomicCounter, DefaultConstructorZero) {
    AtomicCounter c;
    EXPECT_EQ(c.get(), 0);
}

TEST(AtomicCounter, InitialValue) {
    AtomicCounter c(42);
    EXPECT_EQ(c.get(), 42);
}

TEST(AtomicCounter, NegativeInitialValue) {
    AtomicCounter c(-10);
    EXPECT_EQ(c.get(), -10);
}

TEST(AtomicCounter, AddDefault) {
    AtomicCounter c;
    int64_t r = c.add();  // +1
    EXPECT_EQ(r, 1);
    EXPECT_EQ(c.get(), 1);
}

TEST(AtomicCounter, AddDelta) {
    AtomicCounter c(10);
    int64_t r = c.add(5);
    EXPECT_EQ(r, 15);
    EXPECT_EQ(c.get(), 15);
}

TEST(AtomicCounter, SubDefault) {
    AtomicCounter c(10);
    int64_t r = c.sub();  // -1
    EXPECT_EQ(r, 9);
    EXPECT_EQ(c.get(), 9);
}

TEST(AtomicCounter, SubDelta) {
    AtomicCounter c(100);
    int64_t r = c.sub(30);
    EXPECT_EQ(r, 70);
    EXPECT_EQ(c.get(), 70);
}

TEST(AtomicCounter, Set) {
    AtomicCounter c;
    c.set(999);
    EXPECT_EQ(c.get(), 999);
}

TEST(AtomicCounter, Reset) {
    AtomicCounter c(42);
    c.reset();
    EXPECT_EQ(c.get(), 0);
}

TEST(AtomicCounter, PrefixIncrement) {
    AtomicCounter c(5);
    int64_t r = ++c;
    EXPECT_EQ(r, 6);
    EXPECT_EQ(c.get(), 6);
}

TEST(AtomicCounter, PostfixIncrement) {
    AtomicCounter c(5);
    int64_t old = c++;
    EXPECT_EQ(old, 5);
    EXPECT_EQ(c.get(), 6);
}

TEST(AtomicCounter, PrefixDecrement) {
    AtomicCounter c(5);
    int64_t r = --c;
    EXPECT_EQ(r, 4);
    EXPECT_EQ(c.get(), 4);
}

TEST(AtomicCounter, PostfixDecrement) {
    AtomicCounter c(5);
    int64_t old = c--;
    EXPECT_EQ(old, 5);
    EXPECT_EQ(c.get(), 4);
}

TEST(AtomicCounter, ImplicitConversion) {
    AtomicCounter c(77);
    int64_t val = c;
    EXPECT_EQ(val, 77);
}

TEST(AtomicCounter, LargeValues) {
    AtomicCounter c(INT64_MAX - 1);
    c.add(1);
    EXPECT_EQ(c.get(), INT64_MAX);
}

} // namespace fw
} // namespace intertwine
