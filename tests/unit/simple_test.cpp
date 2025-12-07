/**
 * Morphect - Simple Tests (no GoogleTest dependency)
 */

#include "morphect.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

using namespace morphect;

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    try { \
        test_##name(); \
        std::cout << "PASSED" << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << std::endl; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)
#define ASSERT_FALSE(cond) if (cond) throw std::runtime_error("Assertion failed: NOT " #cond)
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)
#define ASSERT_NEAR(a, b, eps) if (std::abs((a) - (b)) > (eps)) throw std::runtime_error("Assertion failed: " #a " ~= " #b)

// ============================================================================
// Random Tests
// ============================================================================

TEST(random_nextInt) {
    Random rng(12345);  // Fixed seed

    for (int i = 0; i < 100; i++) {
        int val = rng.nextInt(0, 10);
        ASSERT_TRUE(val >= 0 && val <= 10);
    }
}

TEST(random_decide) {
    Random rng(12345);

    // Always true
    ASSERT_TRUE(rng.decide(1.0));

    // Always false
    ASSERT_FALSE(rng.decide(0.0));

    // Probabilistic (with fixed seed, should be deterministic)
    int count = 0;
    for (int i = 0; i < 1000; i++) {
        if (rng.decide(0.5)) count++;
    }
    // Should be roughly 50% (allow 40-60%)
    ASSERT_TRUE(count > 400 && count < 600);
}

TEST(random_choose) {
    Random rng(12345);

    std::vector<std::string> items = {"a", "b", "c"};
    std::string chosen = rng.choose(items);

    ASSERT_TRUE(chosen == "a" || chosen == "b" || chosen == "c");
}

TEST(random_weighted_choice) {
    Random rng(12345);

    std::vector<std::string> items = {"rare", "common"};
    std::vector<double> probs = {0.1, 0.9};

    int rare_count = 0;
    for (int i = 0; i < 1000; i++) {
        size_t idx = rng.chooseWeighted(items, probs);
        if (idx == 0) rare_count++;
    }

    // Rare should be around 10% (allow 5-20%)
    ASSERT_TRUE(rare_count > 50 && rare_count < 200);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST(statistics_set_get) {
    Statistics stats;

    stats.set("count", 42);
    stats.set("ratio", 3.14);
    stats.set("name", "test");

    ASSERT_EQ(stats.getInt("count"), 42);
    ASSERT_NEAR(stats.getDouble("ratio"), 3.14, 0.001);
    ASSERT_EQ(stats.getString("name"), "test");
}

TEST(statistics_increment) {
    Statistics stats;

    stats.increment("counter");
    stats.increment("counter");
    stats.increment("counter", 5);

    ASSERT_EQ(stats.getInt("counter"), 7);
}

TEST(statistics_merge) {
    Statistics s1, s2;

    s1.set("a", 10);
    s1.set("b", 20);

    s2.set("a", 5);
    s2.set("c", 30);

    s1.merge(s2);

    ASSERT_EQ(s1.getInt("a"), 15);  // 10 + 5
    ASSERT_EQ(s1.getInt("b"), 20);
    ASSERT_EQ(s1.getInt("c"), 30);
}

// ============================================================================
// JSON Parser Tests
// ============================================================================

TEST(json_parse_string) {
    auto json = JsonParser::parse(R"("hello")");
    ASSERT_TRUE(json.isString());
    ASSERT_EQ(json.asString(), "hello");
}

TEST(json_parse_number) {
    auto json = JsonParser::parse("42");
    ASSERT_TRUE(json.isNumber());
    ASSERT_EQ(json.asInt(), 42);

    json = JsonParser::parse("3.14");
    ASSERT_NEAR(json.asDouble(), 3.14, 0.001);
}

TEST(json_parse_bool) {
    auto t = JsonParser::parse("true");
    auto f = JsonParser::parse("false");

    ASSERT_TRUE(t.isBool());
    ASSERT_TRUE(t.asBool());

    ASSERT_TRUE(f.isBool());
    ASSERT_FALSE(f.asBool());
}

TEST(json_parse_array) {
    auto json = JsonParser::parse("[1, 2, 3]");

    ASSERT_TRUE(json.isArray());
    ASSERT_EQ(json.size(), 3);
    ASSERT_EQ(json[0].asInt(), 1);
    ASSERT_EQ(json[1].asInt(), 2);
    ASSERT_EQ(json[2].asInt(), 3);
}

TEST(json_parse_object) {
    auto json = JsonParser::parse(R"({"name": "test", "value": 42})");

    ASSERT_TRUE(json.isObject());
    ASSERT_TRUE(json.has("name"));
    ASSERT_TRUE(json.has("value"));
    ASSERT_EQ(json["name"].asString(), "test");
    ASSERT_EQ(json["value"].asInt(), 42);
}

TEST(json_nested_access) {
    auto json = JsonParser::parse(R"({
        "settings": {
            "probability": 0.85,
            "enabled": true
        }
    })");

    ASSERT_NEAR(json.get("settings.probability").asDouble(), 0.85, 0.001);
    ASSERT_TRUE(json.get("settings.enabled").asBool());
}

// ============================================================================
// Logger Tests
// ============================================================================

TEST(logger_basic) {
    Logger log("Test");

    // Just ensure these don't crash
    log.trace("trace message");
    log.debug("debug message");
    log.info("info message");
    log.warn("warn message");
    log.error("error message");
}

TEST(logger_format) {
    Logger log("Test");

    // Test format string
    log.info("Value: {}, String: {}", 42, "hello");
}

// ============================================================================
// MBA Verification Tests
// ============================================================================

TEST(mba_add_identity) {
    // Verify: a + b = (a ^ b) + 2 * (a & b)
    for (int a = -100; a <= 100; a += 7) {
        for (int b = -100; b <= 100; b += 7) {
            int expected = a + b;
            int mba = (a ^ b) + 2 * (a & b);
            ASSERT_EQ(expected, mba);
        }
    }
}

TEST(mba_xor_identity) {
    // Verify: a ^ b = (a | b) - (a & b)
    for (int a = -100; a <= 100; a += 7) {
        for (int b = -100; b <= 100; b += 7) {
            int expected = a ^ b;
            int mba = (a | b) - (a & b);
            ASSERT_EQ(expected, mba);
        }
    }
}

TEST(mba_and_identity) {
    // Verify: a & b = (a | b) - (a ^ b)
    for (int a = -100; a <= 100; a += 7) {
        for (int b = -100; b <= 100; b += 7) {
            int expected = a & b;
            int mba = (a | b) - (a ^ b);
            ASSERT_EQ(expected, mba);
        }
    }
}

TEST(mba_or_identity) {
    // Verify: a | b = (a ^ b) + (a & b)
    for (int a = -100; a <= 100; a += 7) {
        for (int b = -100; b <= 100; b += 7) {
            int expected = a | b;
            int mba = (a ^ b) + (a & b);
            ASSERT_EQ(expected, mba);
        }
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "Morphect Unit Tests" << std::endl;
    std::cout << "===================" << std::endl;
    std::cout << std::endl;

    // Random tests
    RUN_TEST(random_nextInt);
    RUN_TEST(random_decide);
    RUN_TEST(random_choose);
    RUN_TEST(random_weighted_choice);

    // Statistics tests
    RUN_TEST(statistics_set_get);
    RUN_TEST(statistics_increment);
    RUN_TEST(statistics_merge);

    // JSON tests
    RUN_TEST(json_parse_string);
    RUN_TEST(json_parse_number);
    RUN_TEST(json_parse_bool);
    RUN_TEST(json_parse_array);
    RUN_TEST(json_parse_object);
    RUN_TEST(json_nested_access);

    // Logger tests
    RUN_TEST(logger_basic);
    RUN_TEST(logger_format);

    // MBA verification tests
    RUN_TEST(mba_add_identity);
    RUN_TEST(mba_xor_identity);
    RUN_TEST(mba_and_identity);
    RUN_TEST(mba_or_identity);

    std::cout << std::endl;
    std::cout << "===================" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    std::cout << "===================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
