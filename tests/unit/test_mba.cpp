/**
 * Morphect - MBA Transformation Tests
 *
 * These tests verify that MBA identities are mathematically correct.
 */

#include <gtest/gtest.h>
#include <cstdint>

// Test the mathematical identities used in MBA transformations

class MBATest : public ::testing::Test {
protected:
    // Test range for exhaustive verification
    static constexpr int MIN_VAL = -100;
    static constexpr int MAX_VAL = 100;
    static constexpr int STEP = 7;  // Step to reduce test count
};

// ============================================================================
// ADD identities: a + b
// ============================================================================

TEST_F(MBATest, AddIdentity_XorAnd) {
    // a + b = (a ^ b) + 2 * (a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a + b;
            int mba = (a ^ b) + 2 * (a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, AddIdentity_OrAnd) {
    // a + b = (a | b) + (a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a + b;
            int mba = (a | b) + (a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, AddIdentity_OrXor) {
    // a + b = 2 * (a | b) - (a ^ b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a + b;
            int mba = 2 * (a | b) - (a ^ b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// XOR identities: a ^ b
// ============================================================================

TEST_F(MBATest, XorIdentity_OrAnd) {
    // a ^ b = (a | b) - (a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a ^ b;
            int mba = (a | b) - (a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, XorIdentity_AddAnd) {
    // a ^ b = (a + b) - 2 * (a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a ^ b;
            int mba = (a + b) - 2 * (a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, XorIdentity_OrNand) {
    // a ^ b = (a | b) & ~(a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a ^ b;
            int mba = (a | b) & ~(a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, XorIdentity_NotAnd) {
    // a ^ b = (~a & b) | (a & ~b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a ^ b;
            int mba = (~a & b) | (a & ~b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// AND identities: a & b
// ============================================================================

TEST_F(MBATest, AndIdentity_OrXor) {
    // a & b = (a | b) - (a ^ b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a & b;
            int mba = (a | b) - (a ^ b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, AndIdentity_DeMorgan) {
    // a & b = ~(~a | ~b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a & b;
            int mba = ~(~a | ~b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// OR identities: a | b
// ============================================================================

TEST_F(MBATest, OrIdentity_XorAnd) {
    // a | b = (a ^ b) + (a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a | b;
            int mba = (a ^ b) + (a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, OrIdentity_AddAnd) {
    // a | b = (a + b) - (a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a | b;
            int mba = (a + b) - (a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, OrIdentity_DeMorgan) {
    // a | b = ~(~a & ~b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a | b;
            int mba = ~(~a & ~b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// SUB identities: a - b
// ============================================================================

TEST_F(MBATest, SubIdentity_TwosComplement) {
    // a - b = a + (~b + 1)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a - b;
            int mba = a + (~b + 1);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, SubIdentity_XorNotAnd) {
    // a - b = (a ^ b) - 2 * (~a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a - b;
            int mba = (a ^ b) - 2 * (~a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_F(MBATest, EdgeCase_Zero) {
    // Test with zero
    EXPECT_EQ(0 + 0, (0 ^ 0) + 2 * (0 & 0));
    EXPECT_EQ(5 + 0, (5 ^ 0) + 2 * (5 & 0));
    EXPECT_EQ(0 + 5, (0 ^ 5) + 2 * (0 & 5));
}

TEST_F(MBATest, EdgeCase_NegativeOne) {
    // Test with -1 (all bits set)
    int a = -1;
    int b = 5;
    EXPECT_EQ(a + b, (a ^ b) + 2 * (a & b));
    EXPECT_EQ(a | b, (a ^ b) + (a & b));
}

TEST_F(MBATest, EdgeCase_PowersOfTwo) {
    // Test with powers of 2
    for (int i = 0; i < 16; i++) {
        int a = 1 << i;
        int b = 1 << ((i + 4) % 16);

        EXPECT_EQ(a + b, (a ^ b) + 2 * (a & b));
        EXPECT_EQ(a | b, (a ^ b) + (a & b));
        EXPECT_EQ(a ^ b, (a | b) - (a & b));
    }
}

// ============================================================================
// Nested MBA identities - 2-level nesting
// ============================================================================

TEST_F(MBATest, NestedAdd_Level2_XorAnd) {
    // a + b = (a ^ b) + 2 * (a & b)
    // Now expand a ^ b = (a | b) - (a & b)
    // And expand a & b = (a | b) - (a ^ b)
    //
    // Level 2: a + b = ((a | b) - (a & b)) + 2 * ((a | b) - (a ^ b))
    // Substituting a & b = (a | b) - (a ^ b) and a ^ b = (a | b) - (a & b)
    // We get complex nested expression

    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a + b;

            // Level 1: (a ^ b) + 2 * (a & b)
            int xor_ab = a ^ b;
            int and_ab = a & b;
            int level1 = xor_ab + 2 * and_ab;

            EXPECT_EQ(expected, level1) << "Level 1 failed for a=" << a << ", b=" << b;

            // Level 2: expand XOR as (a | b) - (a & b)
            // and AND as (a | b) - (a ^ b)
            int or_ab = a | b;
            int xor_expanded = or_ab - and_ab;  // (a | b) - (a & b)
            int and_expanded = or_ab - xor_ab;  // (a | b) - (a ^ b)
            int level2 = xor_expanded + 2 * and_expanded;

            EXPECT_EQ(expected, level2) << "Level 2 failed for a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, NestedXor_Level2_OrAnd) {
    // a ^ b = (a | b) - (a & b)
    // Expand a | b = (a ^ b) + (a & b)
    // Expand a & b = (a | b) - (a ^ b)
    //
    // This creates recursive definitions, but we can substitute one level

    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a ^ b;

            // Level 1: (a | b) - (a & b)
            int or_ab = a | b;
            int and_ab = a & b;
            int level1 = or_ab - and_ab;

            EXPECT_EQ(expected, level1) << "Level 1 failed for a=" << a << ", b=" << b;

            // Level 2: expand OR as (a + b) - (a & b)
            // Keep AND as-is
            int or_expanded = (a + b) - and_ab;  // (a + b) - (a & b)
            int level2 = or_expanded - and_ab;

            EXPECT_EQ(expected, level2) << "Level 2 failed for a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, NestedAnd_Level2_OrXor) {
    // a & b = (a | b) - (a ^ b)
    // Expand a | b = ~(~a & ~b) (De Morgan)
    // Expand a ^ b = (a | b) - (a & b)

    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a & b;

            // Level 1: (a | b) - (a ^ b)
            int or_ab = a | b;
            int xor_ab = a ^ b;
            int level1 = or_ab - xor_ab;

            EXPECT_EQ(expected, level1) << "Level 1 failed for a=" << a << ", b=" << b;

            // Level 2: expand OR using De Morgan: ~(~a & ~b)
            int or_expanded = ~(~a & ~b);
            int level2 = or_expanded - xor_ab;

            EXPECT_EQ(expected, level2) << "Level 2 failed for a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// Nested MBA identities - 3-level nesting
// ============================================================================

TEST_F(MBATest, NestedAdd_Level3) {
    // 3-level nesting of a + b
    // Level 1: (a ^ b) + 2 * (a & b)
    // Level 2: XOR -> (a | b) - (a & b), AND -> (a | b) - (a ^ b)
    // Level 3: Expand OR in the XOR/AND expansions

    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP * 2) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP * 2) {
            int expected = a + b;

            // Raw operations
            int xor_ab = a ^ b;
            int and_ab = a & b;
            int or_ab = a | b;

            // Level 1
            int level1 = xor_ab + 2 * and_ab;
            EXPECT_EQ(expected, level1);

            // Level 2 - substitute XOR and AND
            int xor_l2 = or_ab - and_ab;      // XOR = OR - AND
            int and_l2 = or_ab - xor_ab;      // AND = OR - XOR
            int level2 = xor_l2 + 2 * and_l2;
            EXPECT_EQ(expected, level2);

            // Level 3 - substitute OR in the level 2 expressions
            // OR = (a + b) - (a & b)
            int or_l3 = (a + b) - and_ab;
            int xor_l3 = or_l3 - and_ab;      // XOR = OR' - AND
            int and_l3 = or_l3 - xor_ab;      // AND = OR' - XOR
            int level3 = xor_l3 + 2 * and_l3;
            EXPECT_EQ(expected, level3) << "Level 3 failed for a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// Nested MBA - complex compositions
// ============================================================================

TEST_F(MBATest, NestedComplex_AddSubXor) {
    // Test complex nested operations
    // (a + b) - (a ^ b) should equal 2 * (a & b)

    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = 2 * (a & b);

            // Using MBA expansions
            int add_mba = (a ^ b) + 2 * (a & b);    // a + b
            int xor_mba = (a | b) - (a & b);        // a ^ b
            int result = add_mba - xor_mba;

            EXPECT_EQ(expected, result) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, NestedComplex_ChainedOperations) {
    // Test chained MBA operations
    // ((a + b) ^ c) should produce correct result through nested MBA

    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP * 2) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP * 2) {
            for (int c = MIN_VAL; c <= MAX_VAL; c += STEP * 2) {
                int expected = (a + b) ^ c;

                // First expand a + b via MBA
                int sum = (a ^ b) + 2 * (a & b);  // a + b

                // Then expand XOR with c via MBA
                int result = (sum | c) - (sum & c);  // sum ^ c

                EXPECT_EQ(expected, result) << "a=" << a << ", b=" << b << ", c=" << c;
            }
        }
    }
}

// ============================================================================
// MBA IR Pass configuration tests
// ============================================================================

#include "../../src/passes/mba/mba.hpp"

class MBAPassTest : public ::testing::Test {
protected:
    morphect::mba::LLVMMBAPass pass;
    morphect::mba::MBAPassConfig config;

    void SetUp() override {
        // Enable all transformations with 100% probability
        config.global.enabled = true;
        config.global.probability = 1.0;
        config.global.nesting_depth = 1;
        config.enable_add = true;
        config.enable_sub = true;
        config.enable_xor = true;
        config.enable_and = true;
        config.enable_or = true;
        config.enable_mult = false;  // Keep mult disabled
        pass.initializeMBA(config);
    }
};

TEST_F(MBAPassTest, SingleDepth_TransformsAdd) {
    std::vector<std::string> lines = {
        "  %result = add i32 %a, %b"
    };

    auto result = pass.transformIR(lines);

    EXPECT_EQ(result, morphect::TransformResult::Success);
    EXPECT_GT(lines.size(), 1u);  // Should expand to multiple lines

    // Should no longer have the original simple add at the destination
    // The output should have xor, and, shl, etc.
    bool has_xor = false;
    bool has_and = false;
    for (const auto& line : lines) {
        if (line.find(" xor ") != std::string::npos) has_xor = true;
        if (line.find(" and ") != std::string::npos) has_and = true;
    }

    // At least one of these should be true for ADD transformation
    EXPECT_TRUE(has_xor || has_and);
}

TEST_F(MBAPassTest, Depth2_TransformsNestedOperations) {
    config.global.nesting_depth = 2;
    pass.initializeMBA(config);

    std::vector<std::string> lines = {
        "  %result = add i32 %a, %b"
    };

    auto result = pass.transformIR(lines);

    EXPECT_EQ(result, morphect::TransformResult::Success);
    // With depth 2, we expect even more lines because nested ops get transformed
    EXPECT_GT(lines.size(), 4u);
}

TEST_F(MBAPassTest, Depth3_ProducesMoreExpansion) {
    config.global.nesting_depth = 3;
    pass.initializeMBA(config);

    std::vector<std::string> lines = {
        "  %result = add i32 %a, %b"
    };

    size_t original_size = lines.size();
    auto result = pass.transformIR(lines);

    EXPECT_EQ(result, morphect::TransformResult::Success);
    // Depth 3 should produce even more expansion
    EXPECT_GT(lines.size(), 8u);
}

TEST_F(MBAPassTest, MaxDepthCapped) {
    // Test that depth is capped at 5
    config.global.nesting_depth = 100;  // Try to set very high
    pass.initializeMBA(config);

    std::vector<std::string> lines = {
        "  %result = add i32 %a, %b"
    };

    // Should not explode - internally capped at 5
    auto result = pass.transformIR(lines);
    EXPECT_EQ(result, morphect::TransformResult::Success);
    // Should have expanded but not infinitely
    EXPECT_LT(lines.size(), 1000u);
}

TEST_F(MBAPassTest, DisabledReturnsSkipped) {
    config.global.enabled = false;
    pass.initializeMBA(config);

    std::vector<std::string> lines = {
        "  %result = add i32 %a, %b"
    };

    auto result = pass.transformIR(lines);
    EXPECT_EQ(result, morphect::TransformResult::Skipped);
    EXPECT_EQ(lines.size(), 1u);  // Should not be modified
}

TEST_F(MBAPassTest, NoTransformableLines) {
    std::vector<std::string> lines = {
        "define void @foo() {",
        "  ret void",
        "}"
    };

    auto result = pass.transformIR(lines);
    EXPECT_EQ(result, morphect::TransformResult::NotApplicable);
    EXPECT_EQ(lines.size(), 3u);  // Should not be modified
}

// ============================================================================
// Phase 3.2: Additional MBA Variants Tests
// ============================================================================

TEST_F(MBATest, AddVariant_NegateSub) {
    // a + b = a - (-b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a + b;
            int mba = a - (-b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, AddVariant_NotComplement) {
    // a + b = ~(~a - b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a + b;
            int mba = ~(~a - b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, XorVariant_OrNotOr) {
    // a ^ b = (a | b) & (~a | ~b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a ^ b;
            int mba = (a | b) & (~a | ~b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, XorVariant_OrXorAnd) {
    // a ^ b = (a | b) ^ (a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a ^ b;
            int mba = (a | b) ^ (a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, AndVariant_OrNotXor) {
    // a & b = (a | b) & ~(a ^ b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a & b;
            int mba = (a | b) & ~(a ^ b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, AndVariant_DiffB) {
    // a & b = b - (~a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a & b;
            int mba = b - (~a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, OrVariant_SumDiffB) {
    // a | b = b + (a & ~b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a | b;
            int mba = b + (a & ~b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, OrVariant_AndXor) {
    // a | b = (a & b) | (a ^ b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a | b;
            int mba = (a & b) | (a ^ b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, SubVariant_NotAdd) {
    // a - b = ~(~a + b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a - b;
            int mba = ~(~a + b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

TEST_F(MBATest, SubVariant_DiffExclusive) {
    // a - b = (a & ~b) - (~a & b)
    for (int a = MIN_VAL; a <= MAX_VAL; a += STEP) {
        for (int b = MIN_VAL; b <= MAX_VAL; b += STEP) {
            int expected = a - b;
            int mba = (a & ~b) - (~a & b);
            EXPECT_EQ(expected, mba) << "a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// MBA MULT Tests
// ============================================================================

#include "../../src/passes/mba/mba_mult.hpp"

class MBAMultTest : public ::testing::Test {
protected:
    morphect::mba::LLVMMBAMult mult_transform;
    morphect::mba::MBAConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;
    }
};

// Test helper functions
TEST_F(MBAMultTest, IsPowerOf2) {
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(1));
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(2));
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(4));
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(8));
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(16));
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(32));
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(64));
    EXPECT_TRUE(morphect::mba::LLVMMBAMult::isPowerOf2(1024));

    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(0));
    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(-1));
    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(3));
    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(5));
    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(6));
    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(7));
    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(10));
    EXPECT_FALSE(morphect::mba::LLVMMBAMult::isPowerOf2(15));
}

TEST_F(MBAMultTest, Log2) {
    EXPECT_EQ(0, morphect::mba::LLVMMBAMult::log2(1));
    EXPECT_EQ(1, morphect::mba::LLVMMBAMult::log2(2));
    EXPECT_EQ(2, morphect::mba::LLVMMBAMult::log2(4));
    EXPECT_EQ(3, morphect::mba::LLVMMBAMult::log2(8));
    EXPECT_EQ(4, morphect::mba::LLVMMBAMult::log2(16));
    EXPECT_EQ(5, morphect::mba::LLVMMBAMult::log2(32));
    EXPECT_EQ(10, morphect::mba::LLVMMBAMult::log2(1024));
}

// Test power of 2 multipliers: a * 2^n = a << n
TEST_F(MBAMultTest, PowerOf2_Mul2) {
    std::string line = "  %result = mul i32 %x, 2";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 1") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2_Mul4) {
    std::string line = "  %result = mul i32 %x, 4";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 2") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2_Mul8) {
    std::string line = "  %result = mul i32 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 3") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2_Mul16) {
    std::string line = "  %result = mul i32 %x, 16";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 4") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2_Mul1024) {
    std::string line = "  %result = mul i32 %x, 1024";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 10") != std::string::npos);
}

// Test (2^n - 1) multipliers: a * 7 = (a << 3) - a
TEST_F(MBAMultTest, PowerOf2Minus1_Mul7) {
    std::string line = "  %result = mul i32 %x, 7";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 3") != std::string::npos);  // 2^3 = 8
    EXPECT_TRUE(result[1].find("sub") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2Minus1_Mul15) {
    std::string line = "  %result = mul i32 %x, 15";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 4") != std::string::npos);  // 2^4 = 16
    EXPECT_TRUE(result[1].find("sub") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2Minus1_Mul31) {
    std::string line = "  %result = mul i32 %x, 31";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 5") != std::string::npos);  // 2^5 = 32
    EXPECT_TRUE(result[1].find("sub") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2Minus1_Mul63) {
    std::string line = "  %result = mul i32 %x, 63";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 6") != std::string::npos);  // 2^6 = 64
    EXPECT_TRUE(result[1].find("sub") != std::string::npos);
}

// Test (2^n + 1) multipliers: a * 9 = (a << 3) + a
TEST_F(MBAMultTest, PowerOf2Plus1_Mul9) {
    std::string line = "  %result = mul i32 %x, 9";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 3") != std::string::npos);  // 2^3 = 8
    EXPECT_TRUE(result[1].find("add") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2Plus1_Mul17) {
    std::string line = "  %result = mul i32 %x, 17";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 4") != std::string::npos);  // 2^4 = 16
    EXPECT_TRUE(result[1].find("add") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2Plus1_Mul33) {
    std::string line = "  %result = mul i32 %x, 33";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 5") != std::string::npos);  // 2^5 = 32
    EXPECT_TRUE(result[1].find("add") != std::string::npos);
}

TEST_F(MBAMultTest, PowerOf2Plus1_Mul65) {
    std::string line = "  %result = mul i32 %x, 65";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 6") != std::string::npos);  // 2^6 = 64
    EXPECT_TRUE(result[1].find("add") != std::string::npos);
}

// Test small constants (add chain): 3, 5, 6
// Note: 3 = 4 - 1 = 2^2 - 1, so it uses the (2^n - 1) path: shl 2, then sub
TEST_F(MBAMultTest, SmallConstant_Mul3) {
    std::string line = "  %result = mul i32 %x, 3";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 2") != std::string::npos);  // x << 2 = 4x
    EXPECT_TRUE(result[1].find("sub") != std::string::npos);  // 4x - x = 3x
}

TEST_F(MBAMultTest, SmallConstant_Mul5) {
    std::string line = "  %result = mul i32 %x, 5";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 2") != std::string::npos);  // x << 2
    EXPECT_TRUE(result[1].find("add") != std::string::npos);
}

TEST_F(MBAMultTest, SmallConstant_Mul6) {
    std::string line = "  %result = mul i32 %x, 6";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 2") != std::string::npos);  // x << 2 = 4x
    EXPECT_TRUE(result[1].find("shl") != std::string::npos);
    EXPECT_TRUE(result[1].find(", 1") != std::string::npos);  // x << 1 = 2x
    EXPECT_TRUE(result[2].find("add") != std::string::npos);  // 4x + 2x = 6x
}

// Test decomposition: a * 10 = (a << 3) + (a << 1) = 8a + 2a
TEST_F(MBAMultTest, Decompose_Mul10) {
    std::string line = "  %result = mul i32 %x, 10";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_GE(result.size(), 2u);
    // Should have at least one shl for 8 (<<3) and one for 2 (<<1)
    bool has_shl = false;
    bool has_add = false;
    for (const auto& r : result) {
        if (r.find("shl") != std::string::npos) has_shl = true;
        if (r.find("add") != std::string::npos) has_add = true;
    }
    EXPECT_TRUE(has_shl);
    EXPECT_TRUE(has_add);
}

TEST_F(MBAMultTest, Decompose_Mul11) {
    // 11 = 8 + 2 + 1 = (x << 3) + (x << 1) + x
    std::string line = "  %result = mul i32 %x, 11";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_GE(result.size(), 3u);  // 2 shifts + 2 adds (chained)
    bool has_shl = false;
    bool has_add = false;
    for (const auto& r : result) {
        if (r.find("shl") != std::string::npos) has_shl = true;
        if (r.find("add") != std::string::npos) has_add = true;
    }
    EXPECT_TRUE(has_shl);
    EXPECT_TRUE(has_add);
}

TEST_F(MBAMultTest, Decompose_Mul12) {
    // 12 = 8 + 4 = (x << 3) + (x << 2)
    std::string line = "  %result = mul i32 %x, 12";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_GE(result.size(), 2u);
    bool has_shl = false;
    bool has_add = false;
    for (const auto& r : result) {
        if (r.find("shl") != std::string::npos) has_shl = true;
        if (r.find("add") != std::string::npos) has_add = true;
    }
    EXPECT_TRUE(has_shl);
    EXPECT_TRUE(has_add);
}

TEST_F(MBAMultTest, Decompose_Mul13) {
    // 13 = 8 + 4 + 1 = (x << 3) + (x << 2) + x
    std::string line = "  %result = mul i32 %x, 13";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_GE(result.size(), 3u);
    bool has_shl = false;
    bool has_add = false;
    for (const auto& r : result) {
        if (r.find("shl") != std::string::npos) has_shl = true;
        if (r.find("add") != std::string::npos) has_add = true;
    }
    EXPECT_TRUE(has_shl);
    EXPECT_TRUE(has_add);
}

TEST_F(MBAMultTest, Decompose_Mul14) {
    // 14 = 8 + 4 + 2 = (x << 3) + (x << 2) + (x << 1)
    std::string line = "  %result = mul i32 %x, 14";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_GE(result.size(), 3u);
}

// Test edge cases
TEST_F(MBAMultTest, EdgeCase_Mul1) {
    // a * 1 = a (power of 2 with shift 0)
    std::string line = "  %result = mul i32 %x, 1";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
    EXPECT_TRUE(result[0].find(", 0") != std::string::npos);
}

TEST_F(MBAMultTest, EdgeCase_MulZero) {
    // a * 0 should not be transformed (negative/zero check)
    std::string line = "  %result = mul i32 %x, 0";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());  // No transformation
}

TEST_F(MBAMultTest, EdgeCase_MulNegative) {
    // Negative multiplier should not be transformed
    std::string line = "  %result = mul i32 %x, -5";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());  // No transformation
}

TEST_F(MBAMultTest, EdgeCase_TooManyBits) {
    // Number with >4 bits set that doesn't match special patterns
    // 187 = 10111011 (6 bits set) - not (2^n), (2^n-1), or (2^n+1)
    // 187+1=188, 187-1=186 - neither are powers of 2
    std::string line = "  %result = mul i32 %x, 187";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());  // No transformation (too many additions)
}

// Test non-matching patterns
TEST_F(MBAMultTest, NonMatching_NoConstant) {
    std::string line = "  %result = mul i32 %x, %y";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());  // Only transform with constants
}

TEST_F(MBAMultTest, NonMatching_NotMul) {
    std::string line = "  %result = add i32 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAMultTest, NonMatching_EmptyLine) {
    std::string line = "";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

// Test with nsw/nuw flags
TEST_F(MBAMultTest, WithNsw_PowerOf2) {
    std::string line = "  %result = mul nsw i32 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
}

TEST_F(MBAMultTest, WithNuw_PowerOf2) {
    std::string line = "  %result = mul nuw i32 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
}

TEST_F(MBAMultTest, WithNswNuw_PowerOf2) {
    std::string line = "  %result = mul nsw nuw i32 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
}

// Test different types
TEST_F(MBAMultTest, DifferentType_i64) {
    std::string line = "  %result = mul i64 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("i64") != std::string::npos);
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
}

TEST_F(MBAMultTest, DifferentType_i16) {
    std::string line = "  %result = mul i16 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(result[0].find("i16") != std::string::npos);
    EXPECT_TRUE(result[0].find("shl") != std::string::npos);
}

// Test disabled config
TEST_F(MBAMultTest, Disabled_ReturnsEmpty) {
    config.enabled = false;
    std::string line = "  %result = mul i32 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAMultTest, ZeroProbability_ReturnsEmpty) {
    config.probability = 0.0;
    std::string line = "  %result = mul i32 %x, 8";
    auto result = mult_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

// ============================================================================
// MBA XOR IR Tests - Test all variants of applyIR
// ============================================================================

#include "../../src/passes/mba/mba_xor.hpp"

class MBAXorIRTest : public ::testing::Test {
protected:
    morphect::mba::LLVMMBAXor xor_transform;
    morphect::mba::MBAConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;
    }
};

TEST_F(MBAXorIRTest, Variant0_OrMinusAnd) {
    // Variant 0: (a | b) - (a & b)
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 0, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = or ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = sub ") != std::string::npos);
}

TEST_F(MBAXorIRTest, Variant1_AddMinus2And) {
    // Variant 1: (a + b) - 2 * (a & b)
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 1, config);

    ASSERT_EQ(4u, result.size());
    EXPECT_TRUE(result[0].find(" = add ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = shl ") != std::string::npos);
    EXPECT_TRUE(result[3].find(" = sub ") != std::string::npos);
}

TEST_F(MBAXorIRTest, Variant2_OrAndNotAnd) {
    // Variant 2: (a | b) & ~(a & b)
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 2, config);

    ASSERT_EQ(4u, result.size());
    EXPECT_TRUE(result[0].find(" = or ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);  // NOT
    EXPECT_TRUE(result[3].find(" = and ") != std::string::npos);
}

TEST_F(MBAXorIRTest, Variant3_NotAndOrAndNot) {
    // Variant 3: (~a & b) | (a & ~b)
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 3, config);

    ASSERT_EQ(5u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);  // ~a & b
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[3].find(" = and ") != std::string::npos);  // a & ~b
    EXPECT_TRUE(result[4].find(" = or ") != std::string::npos);
}

TEST_F(MBAXorIRTest, Variant4_OrAndNotOr) {
    // Variant 4: (a | b) & (~a | ~b)
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 4, config);

    ASSERT_EQ(5u, result.size());
    EXPECT_TRUE(result[0].find(" = or ") != std::string::npos);   // a | b
    EXPECT_TRUE(result[1].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[3].find(" = or ") != std::string::npos);   // ~a | ~b
    EXPECT_TRUE(result[4].find(" = and ") != std::string::npos);
}

TEST_F(MBAXorIRTest, Variant5_OrXorAnd) {
    // Variant 5: (a | b) ^ (a & b)
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 5, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = or ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);
}

TEST_F(MBAXorIRTest, DefaultVariant_OutOfRange) {
    // Out of range should use default (variant 5)
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 99, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = or ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);
}

TEST_F(MBAXorIRTest, NonMatching_NotXor) {
    std::string line = "  %result = add i32 %a, %b";
    auto result = xor_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAXorIRTest, NonMatching_XorWithMinusOne) {
    // XOR with -1 is a NOT operation - should not be transformed
    std::string line = "  %result = xor i32 %a, -1";
    auto result = xor_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAXorIRTest, Disabled_ReturnsEmpty) {
    config.enabled = false;
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAXorIRTest, ZeroProbability_ReturnsEmpty) {
    config.probability = 0.0;
    std::string line = "  %result = xor i32 %a, %b";
    auto result = xor_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAXorIRTest, DifferentType_i64) {
    std::string line = "  %result = xor i64 %a, %b";
    auto result = xor_transform.applyIR(line, 0, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find("i64") != std::string::npos);
}

// ============================================================================
// MBA OR IR Tests - Test all variants of applyIR
// ============================================================================

#include "../../src/passes/mba/mba_or.hpp"

class MBAOrIRTest : public ::testing::Test {
protected:
    morphect::mba::LLVMMBAOr or_transform;
    morphect::mba::MBAConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;
    }
};

TEST_F(MBAOrIRTest, Variant0_XorPlusAnd) {
    // Variant 0: (a ^ b) + (a & b)
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 0, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = add ") != std::string::npos);
}

TEST_F(MBAOrIRTest, Variant1_AddMinusAnd) {
    // Variant 1: (a + b) - (a & b)
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 1, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = add ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = sub ") != std::string::npos);
}

TEST_F(MBAOrIRTest, Variant2_DeMorgan) {
    // Variant 2: ~(~a & ~b)
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 2, config);

    ASSERT_EQ(4u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[1].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[2].find(" = and ") != std::string::npos);  // ~a & ~b
    EXPECT_TRUE(result[3].find(" = xor ") != std::string::npos);  // NOT
}

TEST_F(MBAOrIRTest, Variant3_APlusBAndNotA) {
    // Variant 3: a + (b & ~a)
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 3, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);  // b & ~a
    EXPECT_TRUE(result[2].find(" = add ") != std::string::npos);
}

TEST_F(MBAOrIRTest, Variant4_BPlusAAndNotB) {
    // Variant 4: b + (a & ~b)
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 4, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);  // a & ~b
    EXPECT_TRUE(result[2].find(" = add ") != std::string::npos);
}

TEST_F(MBAOrIRTest, Variant5_AndOrXor) {
    // Variant 5: (a & b) | (a ^ b)
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 5, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = and ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = xor ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = or ") != std::string::npos);
}

TEST_F(MBAOrIRTest, DefaultVariant_OutOfRange) {
    // Out of range should use default (variant 5)
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 99, config);

    ASSERT_EQ(3u, result.size());
}

TEST_F(MBAOrIRTest, NonMatching_NotOr) {
    std::string line = "  %result = add i32 %a, %b";
    auto result = or_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAOrIRTest, Disabled_ReturnsEmpty) {
    config.enabled = false;
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAOrIRTest, ZeroProbability_ReturnsEmpty) {
    config.probability = 0.0;
    std::string line = "  %result = or i32 %a, %b";
    auto result = or_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAOrIRTest, DifferentType_i64) {
    std::string line = "  %result = or i64 %a, %b";
    auto result = or_transform.applyIR(line, 0, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find("i64") != std::string::npos);
}

// ============================================================================
// MBA AND IR Tests - Test all variants of applyIR
// ============================================================================

#include "../../src/passes/mba/mba_and.hpp"

class MBAAndIRTest : public ::testing::Test {
protected:
    morphect::mba::LLVMMBAAnd and_transform;
    morphect::mba::MBAConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;
    }
};

TEST_F(MBAAndIRTest, Variant0_OrMinusXor) {
    // Variant 0: (a | b) - (a ^ b)
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 0, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = or ") != std::string::npos);
    EXPECT_TRUE(result[1].find(" = xor ") != std::string::npos);
    EXPECT_TRUE(result[2].find(" = sub ") != std::string::npos);
}

TEST_F(MBAAndIRTest, Variant1_DeMorgan) {
    // Variant 1: ~(~a | ~b)
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 1, config);

    ASSERT_EQ(4u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[1].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[2].find(" = or ") != std::string::npos);   // ~a | ~b
    EXPECT_TRUE(result[3].find(" = xor ") != std::string::npos);  // NOT
}

TEST_F(MBAAndIRTest, Variant2_AMinusAAndNotB) {
    // Variant 2: a - (a & ~b)
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 2, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);  // a & ~b
    EXPECT_TRUE(result[2].find(" = sub ") != std::string::npos);
}

TEST_F(MBAAndIRTest, Variant3_OrAndNotXor) {
    // Variant 3: (a | b) & ~(a ^ b)
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 3, config);

    ASSERT_EQ(4u, result.size());
    EXPECT_TRUE(result[0].find(" = or ") != std::string::npos);   // a | b
    EXPECT_TRUE(result[1].find(" = xor ") != std::string::npos);  // a ^ b
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);  // NOT
    EXPECT_TRUE(result[3].find(" = and ") != std::string::npos);
}

TEST_F(MBAAndIRTest, Variant4_BMinusNotAAndB) {
    // Variant 4: b - (~a & b)
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 4, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);  // ~a & b
    EXPECT_TRUE(result[2].find(" = sub ") != std::string::npos);
}

TEST_F(MBAAndIRTest, DefaultVariant_OutOfRange) {
    // Out of range should use default (variant 4)
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 99, config);

    ASSERT_EQ(3u, result.size());
}

TEST_F(MBAAndIRTest, NonMatching_NotAnd) {
    std::string line = "  %result = add i32 %a, %b";
    auto result = and_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAAndIRTest, Disabled_ReturnsEmpty) {
    config.enabled = false;
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAAndIRTest, ZeroProbability_ReturnsEmpty) {
    config.probability = 0.0;
    std::string line = "  %result = and i32 %a, %b";
    auto result = and_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBAAndIRTest, DifferentType_i64) {
    std::string line = "  %result = and i64 %a, %b";
    auto result = and_transform.applyIR(line, 0, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find("i64") != std::string::npos);
}

// ============================================================================
// MBA SUB IR Tests - Test all variants of applyIR
// ============================================================================

#include "../../src/passes/mba/mba_sub.hpp"

class MBASubIRTest : public ::testing::Test {
protected:
    morphect::mba::LLVMMBASub sub_transform;
    morphect::mba::MBAConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;
    }
};

TEST_F(MBASubIRTest, Variant0_XorMinus2NotAAndB) {
    // Variant 0: (a ^ b) - 2 * (~a & b)
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    ASSERT_EQ(5u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // a ^ b
    EXPECT_TRUE(result[1].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[2].find(" = and ") != std::string::npos);  // ~a & b
    EXPECT_TRUE(result[3].find(" = shl ") != std::string::npos);  // * 2
    EXPECT_TRUE(result[4].find(" = sub ") != std::string::npos);
}

TEST_F(MBASubIRTest, Variant1_OrNotBMinusOrNotA) {
    // Variant 1: (a | ~b) - (~a | b) + 1
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 1, config);

    ASSERT_EQ(6u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[1].find(" = or ") != std::string::npos);   // a | ~b
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[3].find(" = or ") != std::string::npos);   // ~a | b
    EXPECT_TRUE(result[4].find(" = sub ") != std::string::npos);
    EXPECT_TRUE(result[5].find(" = add ") != std::string::npos);  // + 1
}

TEST_F(MBASubIRTest, Variant2_TwosComplement) {
    // Variant 2: a + (~b + 1)
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 2, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[1].find(" = add ") != std::string::npos);  // ~b + 1
    EXPECT_TRUE(result[2].find(" = add ") != std::string::npos);  // a + (-b)
}

TEST_F(MBASubIRTest, Variant3_NotNotAPlusB) {
    // Variant 3: ~(~a + b)
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 3, config);

    ASSERT_EQ(3u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[1].find(" = add ") != std::string::npos);  // ~a + b
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);  // NOT
}

TEST_F(MBASubIRTest, Variant4_AAndNotBMinusNotAAndB) {
    // Variant 4: (a & ~b) - (~a & b)
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 4, config);

    ASSERT_EQ(5u, result.size());
    EXPECT_TRUE(result[0].find(" = xor ") != std::string::npos);  // ~b
    EXPECT_TRUE(result[1].find(" = and ") != std::string::npos);  // a & ~b
    EXPECT_TRUE(result[2].find(" = xor ") != std::string::npos);  // ~a
    EXPECT_TRUE(result[3].find(" = and ") != std::string::npos);  // ~a & b
    EXPECT_TRUE(result[4].find(" = sub ") != std::string::npos);
}

TEST_F(MBASubIRTest, DefaultVariant_OutOfRange) {
    // Out of range should use default (variant 4)
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 99, config);

    ASSERT_EQ(5u, result.size());
}

TEST_F(MBASubIRTest, NonMatching_NotSub) {
    std::string line = "  %result = add i32 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBASubIRTest, Disabled_ReturnsEmpty) {
    config.enabled = false;
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBASubIRTest, ZeroProbability_ReturnsEmpty) {
    config.probability = 0.0;
    std::string line = "  %result = sub i32 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    EXPECT_TRUE(result.empty());
}

TEST_F(MBASubIRTest, DifferentType_i64) {
    std::string line = "  %result = sub i64 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    ASSERT_EQ(5u, result.size());
    EXPECT_TRUE(result[0].find("i64") != std::string::npos);
}

TEST_F(MBASubIRTest, WithNswFlag) {
    std::string line = "  %result = sub nsw i32 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    ASSERT_EQ(5u, result.size());
}

TEST_F(MBASubIRTest, WithNuwFlag) {
    std::string line = "  %result = sub nuw i32 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    ASSERT_EQ(5u, result.size());
}

TEST_F(MBASubIRTest, WithNswNuwFlags) {
    std::string line = "  %result = sub nsw nuw i32 %a, %b";
    auto result = sub_transform.applyIR(line, 0, config);

    ASSERT_EQ(5u, result.size());
}
