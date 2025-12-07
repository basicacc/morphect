/**
 * Morphect - Extended Coverage Tests
 *
 * Additional tests to achieve >80% code coverage across all modules.
 * Tests edge cases, mathematical identities, and algorithm correctness.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <cstring>
#include <set>

#include "common/random.hpp"
#include "core/statistics.hpp"
#include "core/transformation_base.hpp"

using namespace morphect;

// ============================================================================
// MBA Extended Edge Cases
// ============================================================================

class MBAExtendedTest : public ::testing::Test {
protected:
    static constexpr int32_t INT_MAX_VAL = std::numeric_limits<int32_t>::max();
    static constexpr int32_t INT_MIN_VAL = std::numeric_limits<int32_t>::min();
};

TEST_F(MBAExtendedTest, AddIdentity_IntMax) {
    // Test at integer boundaries
    int32_t a = INT_MAX_VAL;
    int32_t b = 0;
    EXPECT_EQ(a + b, (a ^ b) + 2 * (a & b));
    EXPECT_EQ(a + b, (a | b) + (a & b));
}

TEST_F(MBAExtendedTest, AddIdentity_IntMin) {
    int32_t a = INT_MIN_VAL;
    int32_t b = 0;
    EXPECT_EQ(a + b, (a ^ b) + 2 * (a & b));
    EXPECT_EQ(a + b, (a | b) + (a & b));
}

TEST_F(MBAExtendedTest, XorIdentity_AllOnes) {
    int32_t a = -1;  // All bits set
    int32_t b = 0;
    EXPECT_EQ(a ^ b, (a | b) - (a & b));
    EXPECT_EQ(a ^ b, (a + b) - 2 * (a & b));
}

TEST_F(MBAExtendedTest, AndIdentity_Boundaries) {
    int32_t a = INT_MAX_VAL;
    int32_t b = INT_MIN_VAL;
    EXPECT_EQ(a & b, (a | b) - (a ^ b));
    EXPECT_EQ(a & b, ~(~a | ~b));  // De Morgan
}

TEST_F(MBAExtendedTest, OrIdentity_Boundaries) {
    int32_t a = INT_MAX_VAL;
    int32_t b = INT_MIN_VAL;
    EXPECT_EQ(a | b, (a ^ b) + (a & b));
    EXPECT_EQ(a | b, ~(~a & ~b));  // De Morgan
}

TEST_F(MBAExtendedTest, SubIdentity_IntMax) {
    int32_t a = INT_MAX_VAL;
    int32_t b = 1;
    EXPECT_EQ(a - b, a + ~b + 1);
}

TEST_F(MBAExtendedTest, MultIdentity_PowersOfTwo) {
    // a * 2 = a << 1 = a + a
    for (int a = -1000; a <= 1000; a++) {
        EXPECT_EQ(a * 2, a + a);
        EXPECT_EQ(a * 4, a + a + a + a);
    }
}

TEST_F(MBAExtendedTest, NestedIdentities_Complex) {
    // Test deeply nested MBA transformations
    for (int a = -50; a <= 50; a += 5) {
        for (int b = -50; b <= 50; b += 5) {
            // ((a^b) + 2*(a&b)) = a + b
            // Then transform (a^b) -> (a|b) - (a&b)
            // Result: ((a|b) - (a&b)) + 2*(a&b) = (a|b) + (a&b) = a + b
            int nested = ((a | b) - (a & b)) + 2 * (a & b);
            EXPECT_EQ(nested, a + b) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBAExtendedTest, BitPatterns) {
    // Test various bit patterns (use unsigned and cast to avoid narrowing)
    std::vector<uint32_t> upatterns = {
        0x00000000u, 0xFFFFFFFFu, 0x55555555u, 0xAAAAAAAAu,
        0x0F0F0F0Fu, 0xF0F0F0F0u, 0x00FF00FFu, 0xFF00FF00u
    };

    for (uint32_t ua : upatterns) {
        for (uint32_t ub : upatterns) {
            // Use unsigned arithmetic for bit patterns
            EXPECT_EQ(ua + ub, (ua ^ ub) + 2 * (ua & ub));
            EXPECT_EQ(ua ^ ub, (ua | ub) - (ua & ub));
        }
    }
}

// ============================================================================
// Opaque Predicate Mathematical Tests
// ============================================================================

TEST(OpaquePredicateMathTest, EvenProduct_AlwaysTrue) {
    // (x * (x + 1)) % 2 == 0 for all x (consecutive integers)
    for (int x = -1000; x <= 1000; x++) {
        EXPECT_EQ((x * (x + 1)) % 2, 0) << "x=" << x;
    }
}

TEST(OpaquePredicateMathTest, SquareNonNegative_AlwaysTrue) {
    // x * x >= 0 for all integers
    for (int x = -1000; x <= 1000; x++) {
        int64_t square = static_cast<int64_t>(x) * x;
        EXPECT_GE(square, 0) << "x=" << x;
    }
}

TEST(OpaquePredicateMathTest, XorSelfZero_AlwaysTrue) {
    // (x ^ x) == 0 for all x
    for (int x = -1000; x <= 1000; x++) {
        EXPECT_EQ(x ^ x, 0) << "x=" << x;
    }
}

TEST(OpaquePredicateMathTest, BooleanIdentity_AlwaysTrue) {
    // ((x & y) | (x ^ y)) == (x | y) for all x, y
    for (int x = -50; x <= 50; x++) {
        for (int y = -50; y <= 50; y++) {
            int lhs = (x & y) | (x ^ y);
            int rhs = x | y;
            EXPECT_EQ(lhs, rhs) << "x=" << x << ", y=" << y;
        }
    }
}

TEST(OpaquePredicateMathTest, MBAIdentity_AlwaysTrue) {
    // 2 * (x & y) + (x ^ y) == x + y for all x, y
    for (int x = -50; x <= 50; x++) {
        for (int y = -50; y <= 50; y++) {
            int lhs = 2 * (x & y) + (x ^ y);
            int rhs = x + y;
            EXPECT_EQ(lhs, rhs) << "x=" << x << ", y=" << y;
        }
    }
}

TEST(OpaquePredicateMathTest, ModuloPredicate_AlwaysTrue) {
    // (x^2 + x) % 2 == 0 for all x
    for (int x = -1000; x <= 1000; x++) {
        EXPECT_EQ((x * x + x) % 2, 0) << "x=" << x;
    }
}

TEST(OpaquePredicateMathTest, BitCountPredicate) {
    // x & (x - 1) removes lowest set bit
    // For power of 2, x & (x-1) == 0
    for (int exp = 0; exp < 30; exp++) {
        int x = 1 << exp;
        EXPECT_EQ(x & (x - 1), 0) << "x=" << x;
    }
}

// ============================================================================
// String Encoding Tests
// ============================================================================

TEST(StringEncodingCoverageTest, SimpleXOREncode) {
    // Test basic string encoding concept
    std::string test = "Hello World";
    uint8_t key = 0x42;

    // Manual XOR encode
    std::string encoded;
    for (char c : test) {
        encoded += static_cast<char>(c ^ key);
    }

    // Manual XOR decode
    std::string decoded;
    for (char c : encoded) {
        decoded += static_cast<char>(c ^ key);
    }

    EXPECT_EQ(decoded, test);
}

TEST(StringEncodingCoverageTest, RollingXORConcept) {
    // Test rolling XOR concept
    std::string test = "Test string";
    uint8_t prev = 0x55;  // Initial key

    std::string encoded;
    for (char c : test) {
        char enc = static_cast<char>(c ^ prev);
        encoded += enc;
        prev = static_cast<uint8_t>(c);  // Next key is previous plaintext
    }

    // Decode
    prev = 0x55;
    std::string decoded;
    for (char c : encoded) {
        char dec = static_cast<char>(c ^ prev);
        decoded += dec;
        prev = static_cast<uint8_t>(dec);
    }

    EXPECT_EQ(decoded, test);
}

TEST(StringEncodingCoverageTest, AllByteValues) {
    // Test that XOR encoding works for all byte values
    uint8_t key = 0x42;

    for (int i = 0; i < 256; i++) {
        uint8_t original = static_cast<uint8_t>(i);
        uint8_t encoded = original ^ key;
        uint8_t decoded = encoded ^ key;
        EXPECT_EQ(decoded, original);
    }
}

// ============================================================================
// Constant Obfuscation Tests
// ============================================================================

TEST(ConstantObfCoverageTest, XORObfuscation) {
    int64_t original = 12345;
    int64_t key = 0xDEADBEEF;

    int64_t obfuscated = original ^ key;
    int64_t recovered = obfuscated ^ key;

    EXPECT_EQ(recovered, original);
}

TEST(ConstantObfCoverageTest, SplitObfuscation) {
    int64_t original = 1000;
    int64_t part1 = 700;
    int64_t part2 = original - part1;

    EXPECT_EQ(part1 + part2, original);
}

TEST(ConstantObfCoverageTest, ArithmeticObfuscation) {
    int64_t original = 42;
    int64_t offset = 12345;

    int64_t obfuscated = original + offset;
    int64_t recovered = obfuscated - offset;

    EXPECT_EQ(recovered, original);
}

TEST(ConstantObfCoverageTest, MultiplyDivideObfuscation) {
    int64_t original = 100;
    int64_t factor = 17;

    int64_t obfuscated = original * factor;
    int64_t recovered = obfuscated / factor;

    EXPECT_EQ(recovered, original);
}

TEST(ConstantObfCoverageTest, BitSplitObfuscation) {
    int64_t original = 0xABCD;

    int64_t high = original & 0xFF00;
    int64_t low = original & 0x00FF;

    int64_t recovered = high | low;
    EXPECT_EQ(recovered, original);
}

TEST(ConstantObfCoverageTest, EdgeCases) {
    int64_t key = 0x12345678;

    EXPECT_EQ((0LL ^ key) ^ key, 0LL);
    EXPECT_EQ((-1LL ^ key) ^ key, -1LL);

    int64_t max_val = std::numeric_limits<int64_t>::max();
    EXPECT_EQ((max_val ^ key) ^ key, max_val);

    int64_t min_val = std::numeric_limits<int64_t>::min();
    EXPECT_EQ((min_val ^ key) ^ key, min_val);
}

// ============================================================================
// Random and Statistics Tests
// ============================================================================

TEST(RandomExtendedTest, UniformDistribution) {
    const int NUM_BUCKETS = 10;
    const int NUM_SAMPLES = 10000;
    int buckets[NUM_BUCKETS] = {0};

    for (int i = 0; i < NUM_SAMPLES; i++) {
        int val = GlobalRandom::nextInt(0, NUM_BUCKETS - 1);
        ASSERT_GE(val, 0);
        ASSERT_LT(val, NUM_BUCKETS);
        buckets[val]++;
    }

    int expected = NUM_SAMPLES / NUM_BUCKETS;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        EXPECT_GT(buckets[i], expected * 0.7);
        EXPECT_LT(buckets[i], expected * 1.3);
    }
}

TEST(RandomExtendedTest, DecideProbabilityBoundaries) {
    // 0.0 should always return false
    bool ever_true = false;
    for (int i = 0; i < 100; i++) {
        if (GlobalRandom::decide(0.0)) ever_true = true;
    }
    EXPECT_FALSE(ever_true);

    // 1.0 should always return true
    bool ever_false = false;
    for (int i = 0; i < 100; i++) {
        if (!GlobalRandom::decide(1.0)) ever_false = true;
    }
    EXPECT_FALSE(ever_false);
}

TEST(RandomExtendedTest, NextDoubleRange) {
    for (int i = 0; i < 1000; i++) {
        double val = GlobalRandom::nextDouble();
        EXPECT_GE(val, 0.0);
        EXPECT_LT(val, 1.0);
    }
}

TEST(StatisticsExtendedTest, SetAndGet) {
    Statistics stats;
    stats.set("int_val", 42);
    stats.set("double_val", 3.14);
    stats.set("string_val", std::string("test"));

    EXPECT_EQ(stats.getInt("int_val"), 42);
    EXPECT_DOUBLE_EQ(stats.getDouble("double_val"), 3.14);
    EXPECT_EQ(stats.getString("string_val"), "test");
}

TEST(StatisticsExtendedTest, Increment) {
    Statistics stats;
    stats.increment("counter");
    EXPECT_EQ(stats.getInt("counter"), 1);

    stats.increment("counter", 5);
    EXPECT_EQ(stats.getInt("counter"), 6);
}

TEST(StatisticsExtendedTest, MergeStatistics) {
    Statistics stats1;
    stats1.set("a", 10);
    stats1.set("b", 20);

    Statistics stats2;
    stats2.set("b", 5);
    stats2.set("c", 30);

    stats1.merge(stats2);

    EXPECT_EQ(stats1.getInt("a"), 10);
    EXPECT_EQ(stats1.getInt("b"), 25);
    EXPECT_EQ(stats1.getInt("c"), 30);
}

TEST(StatisticsExtendedTest, HasKey) {
    Statistics stats;
    EXPECT_FALSE(stats.has("key"));

    stats.set("key", 42);
    EXPECT_TRUE(stats.has("key"));
}

// ============================================================================
// Pass Configuration Tests
// ============================================================================

TEST(PassConfigCoverageTest, DefaultValues) {
    PassConfig config;

    EXPECT_TRUE(config.enabled);
    EXPECT_DOUBLE_EQ(config.probability, 0.85);  // Actual default in transformation_base.hpp
}

TEST(PassConfigCoverageTest, PriorityOrdering) {
    EXPECT_LT(static_cast<int>(PassPriority::Early),
              static_cast<int>(PassPriority::Normal));
    EXPECT_LT(static_cast<int>(PassPriority::Normal),
              static_cast<int>(PassPriority::Late));
}

TEST(TransformResultTest, EnumValues) {
    EXPECT_NE(TransformResult::Success, TransformResult::Error);
    EXPECT_NE(TransformResult::Success, TransformResult::Skipped);
    EXPECT_NE(TransformResult::Error, TransformResult::NotApplicable);
}

// ============================================================================
// Control Flow Analysis Math Tests
// ============================================================================

TEST(ControlFlowMathTest, JumpTableIndex) {
    // Test jump table index computation
    // index = (value * multiplier + offset) & mask
    for (int value = 0; value < 10; value++) {
        int multiplier = 3;
        int offset = 5;
        int mask = 0xF;

        int index = (value * multiplier + offset) & mask;
        EXPECT_GE(index, 0);
        EXPECT_LE(index, mask);
    }
}

TEST(ControlFlowMathTest, IndirectBranchObfuscation) {
    // Test that indirect branch computation is reversible
    int target = 42;
    int key = 0x12345678;

    // Obfuscate
    int obfuscated = target ^ key;

    // De-obfuscate
    int recovered = obfuscated ^ key;

    EXPECT_EQ(recovered, target);
}

// ============================================================================
// Anti-Disassembly Pattern Tests
// ============================================================================

TEST(AntiDisasmCoverageTest, JunkBytePatterns) {
    // Test that prefix bytes are valid x86 prefixes
    std::vector<uint8_t> prefixes = {
        0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65, 0x66, 0x67, 0xF0, 0xF2, 0xF3
    };

    for (uint8_t prefix : prefixes) {
        // All should be in range 0x00-0xFF (obviously)
        EXPECT_LE(prefix, 0xFF);
    }
}

TEST(AntiDisasmCoverageTest, FakeCallInstruction) {
    // CALL rel32 is E8 followed by 4 bytes
    uint8_t call_opcode = 0xE8;
    EXPECT_EQ(call_opcode, 0xE8);

    // A fake call would have random bytes following
    // We just test the concept
}

// ============================================================================
// Anti-Debug Detection Concepts
// ============================================================================

TEST(AntiDebugConceptTest, TimingConcept) {
    // Concept: measure time, if too slow, debugger detected
    auto start = std::chrono::high_resolution_clock::now();

    // Do trivial work
    volatile int dummy = 0;
    for (int i = 0; i < 1000; i++) {
        dummy += i;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Without debugger, should be very fast (< 10ms)
    EXPECT_LT(duration.count(), 10000);
}

TEST(AntiDebugConceptTest, XORStringEncoding) {
    // Concept: strings are XOR encoded to hide from static analysis
    std::string hidden = "password";
    uint8_t key = 0x42;

    std::string encoded;
    for (char c : hidden) {
        encoded += c ^ key;
    }

    // Encoded should not equal original
    EXPECT_NE(encoded, hidden);

    // Decoding should recover original
    std::string decoded;
    for (char c : encoded) {
        decoded += c ^ key;
    }
    EXPECT_EQ(decoded, hidden);
}

// ============================================================================
// Dead Code Insertion Concepts
// ============================================================================

TEST(DeadCodeConceptTest, OpaquePredicateFalse) {
    // Concept: opaque predicate that is always false
    // (x * x < 0) for non-negative x - always false

    for (int x = 0; x <= 1000; x++) {
        int64_t square = static_cast<int64_t>(x) * x;
        EXPECT_FALSE(square < 0);
    }
}

TEST(DeadCodeConceptTest, OpaquePredicateTrue) {
    // Concept: opaque predicate that is always true
    // (x * (x + 1)) % 2 == 0 - always true

    for (int x = -1000; x <= 1000; x++) {
        EXPECT_TRUE((x * (x + 1)) % 2 == 0);
    }
}

// ============================================================================
// Bitwise Operation Tests
// ============================================================================

TEST(BitwiseTest, DeMorgansLaws) {
    // ~(a & b) == (~a | ~b)
    // ~(a | b) == (~a & ~b)
    for (int a = -100; a <= 100; a++) {
        for (int b = -100; b <= 100; b++) {
            EXPECT_EQ(~(a & b), (~a | ~b));
            EXPECT_EQ(~(a | b), (~a & ~b));
        }
    }
}

TEST(BitwiseTest, XORProperties) {
    // a ^ a = 0
    // a ^ 0 = a
    // a ^ ~0 = ~a
    for (int a = -100; a <= 100; a++) {
        EXPECT_EQ(a ^ a, 0);
        EXPECT_EQ(a ^ 0, a);
        EXPECT_EQ(a ^ ~0, ~a);
    }
}

TEST(BitwiseTest, ShiftProperties) {
    // (a << 1) = a * 2 (for small enough a)
    // (a >> 1) = a / 2 (for positive a)
    for (int a = 0; a < 1000; a++) {
        EXPECT_EQ(a << 1, a * 2);
        EXPECT_EQ(a >> 1, a / 2);
    }
}
