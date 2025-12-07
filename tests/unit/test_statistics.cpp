/**
 * Morphect - Statistics Tests
 */

#include <gtest/gtest.h>
#include "core/statistics.hpp"

using namespace morphect;

TEST(StatisticsTest, SetAndGet) {
    Statistics stats;

    stats.set("count", 42);
    stats.set("ratio", 3.14);
    stats.set("name", "test");

    EXPECT_EQ(stats.getInt("count"), 42);
    EXPECT_NEAR(stats.getDouble("ratio"), 3.14, 0.001);
    EXPECT_EQ(stats.getString("name"), "test");
}

TEST(StatisticsTest, Increment) {
    Statistics stats;

    stats.increment("counter");
    stats.increment("counter");
    stats.increment("counter", 5);

    EXPECT_EQ(stats.getInt("counter"), 7);
}

TEST(StatisticsTest, DefaultValues) {
    Statistics stats;

    // Non-existent keys should return defaults
    EXPECT_EQ(stats.getInt("nonexistent"), 0);
    EXPECT_DOUBLE_EQ(stats.getDouble("nonexistent"), 0.0);
    EXPECT_EQ(stats.getString("nonexistent"), "");
}

TEST(StatisticsTest, Merge) {
    Statistics s1, s2;

    s1.set("a", 10);
    s1.set("b", 20);

    s2.set("a", 5);
    s2.set("c", 30);

    s1.merge(s2);

    EXPECT_EQ(s1.getInt("a"), 15);  // 10 + 5
    EXPECT_EQ(s1.getInt("b"), 20);
    EXPECT_EQ(s1.getInt("c"), 30);
}

TEST(StatisticsTest, Clear) {
    Statistics stats;

    stats.set("count", 42);
    stats.clear();

    EXPECT_EQ(stats.getInt("count"), 0);
}

TEST(StatisticsTest, TotalTransformations) {
    Statistics stats;

    stats.set("mba_add_applied", 10);
    stats.set("mba_xor_applied", 5);
    stats.set("other_transform", 3);

    int total = stats.getTotalTransformations();

    // Should count stats with "applied" or "transform" in name
    EXPECT_EQ(total, 18);
}

TEST(StatisticsTest, Has) {
    Statistics stats;

    stats.set("exists", 1);

    EXPECT_TRUE(stats.has("exists"));
    EXPECT_FALSE(stats.has("nonexistent"));
}

TEST(TimerTest, BasicTiming) {
    Timer timer;

    timer.start();
    // Do some work
    volatile int x = 0;
    for (int i = 0; i < 100000; i++) x += i;
    timer.stop();

    double elapsed = timer.elapsedMs();
    EXPECT_GT(elapsed, 0.0);
}

TEST(ScopedTimerTest, AutoRecord) {
    double target = 0.0;

    {
        ScopedTimer timer(target);
        // Do some work
        volatile int x = 0;
        for (int i = 0; i < 100000; i++) x += i;
    }

    EXPECT_GT(target, 0.0);
}
