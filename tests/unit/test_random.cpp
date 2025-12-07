/**
 * Morphect - Random Number Generator Tests
 */

#include <gtest/gtest.h>
#include "common/random.hpp"

using namespace morphect;

TEST(RandomTest, NextIntRange) {
    Random rng(12345);

    for (int i = 0; i < 100; i++) {
        int val = rng.nextInt(0, 10);
        EXPECT_GE(val, 0);
        EXPECT_LE(val, 10);
    }
}

TEST(RandomTest, DecideAlways) {
    Random rng(12345);

    // Always true
    EXPECT_TRUE(rng.decide(1.0));

    // Always false
    EXPECT_FALSE(rng.decide(0.0));
}

TEST(RandomTest, DecideProbability) {
    Random rng(12345);

    int count = 0;
    for (int i = 0; i < 1000; i++) {
        if (rng.decide(0.5)) count++;
    }

    // Should be roughly 50% (allow 40-60%)
    EXPECT_GT(count, 400);
    EXPECT_LT(count, 600);
}

TEST(RandomTest, ChooseFromVector) {
    Random rng(12345);

    std::vector<std::string> items = {"a", "b", "c"};
    std::string chosen = rng.choose(items);

    EXPECT_TRUE(chosen == "a" || chosen == "b" || chosen == "c");
}

TEST(RandomTest, WeightedChoice) {
    Random rng(12345);

    std::vector<std::string> items = {"rare", "common"};
    std::vector<double> probs = {0.1, 0.9};

    int rare_count = 0;
    for (int i = 0; i < 1000; i++) {
        size_t idx = rng.chooseWeighted(items, probs);
        if (idx == 0) rare_count++;
    }

    // Rare should be around 10% (allow 5-20%)
    EXPECT_GT(rare_count, 50);
    EXPECT_LT(rare_count, 200);
}

TEST(RandomTest, Reproducibility) {
    Random rng1(42);
    Random rng2(42);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(rng1.nextInt(0, 1000), rng2.nextInt(0, 1000));
    }
}

TEST(GlobalRandomTest, BasicUsage) {
    GlobalRandom::setSeed(12345);

    int val = GlobalRandom::nextInt(0, 100);
    EXPECT_GE(val, 0);
    EXPECT_LE(val, 100);
}
