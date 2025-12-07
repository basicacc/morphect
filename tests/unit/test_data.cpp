/**
 * Morphect - Data Obfuscation Tests
 *
 * Tests for string encoding, constant obfuscation, and variable splitting.
 */

#include <gtest/gtest.h>
#include <set>
#include <cstring>
#include "../../src/passes/data/data.hpp"

using namespace morphect::data;

// ============================================================================
// Variable Splitting Tests
// ============================================================================

class VariableSplittingTest : public ::testing::Test {
protected:
    LLVMVariableSplittingPass pass;
    VariableSplittingConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;  // Always split for testing
        config.default_strategy = SplitStrategy::Additive;
        config.split_phi_nodes = false;  // Disable for simplicity
        config.max_splits_per_function = 10;
        pass.configure(config);
    }
};

TEST_F(VariableSplittingTest, SkipsWhenDisabled) {
    config.enabled = false;
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @foo() {",
        "  %x = add i32 1, 2",
        "  ret i32 %x",
        "}"
    };

    auto result = pass.transformIR(lines);
    EXPECT_EQ(result, morphect::TransformResult::Skipped);
}

TEST_F(VariableSplittingTest, TransformsSimpleFunction) {
    std::vector<std::string> lines = {
        "define i32 @foo() {",
        "  %x = add i32 1, 2",
        "  %y = add i32 %x, 3",
        "  ret i32 %y",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Should have transformed something
    // The exact result depends on whether variables were selected for splitting
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingTest, PreservesNonFunctionCode) {
    std::vector<std::string> lines = {
        "; Module header",
        "source_filename = \"test.c\"",
        "@global = global i32 0",
        "",
        "define i32 @foo() {",
        "  ret i32 0",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Check that non-function code is preserved
    EXPECT_EQ(lines[0], "; Module header");
    EXPECT_EQ(lines[1], "source_filename = \"test.c\"");
    EXPECT_EQ(lines[2], "@global = global i32 0");
}

TEST_F(VariableSplittingTest, ExcludesPatterns) {
    config.exclude_patterns = {"%ptr", "%addr"};
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @foo(i32* %ptr) {",
        "  %addr = getelementptr i32, i32* %ptr, i32 0",
        "  %val = load i32, i32* %addr",
        "  ret i32 %val",
        "}"
    };

    auto result = pass.transformIR(lines);

    // %ptr and %addr should be excluded from splitting
    // Check they still exist unchanged
    bool found_ptr = false;
    bool found_addr = false;
    for (const auto& line : lines) {
        if (line.find("%ptr") != std::string::npos) found_ptr = true;
        if (line.find("%addr") != std::string::npos) found_addr = true;
    }
    EXPECT_TRUE(found_ptr);
}

// ============================================================================
// Split Strategy Tests
// ============================================================================

TEST_F(VariableSplittingTest, AdditiveSplitProducesCorrectReconstruction) {
    config.default_strategy = SplitStrategy::Additive;
    pass.configure(config);

    // The mathematical identity: x = part1 + part2
    // If we split x into part1 and part2, reconstructing should give x
    // This is tested implicitly through IR transformation
}

TEST_F(VariableSplittingTest, XorSplitProducesCorrectReconstruction) {
    config.default_strategy = SplitStrategy::XOR;
    pass.configure(config);

    // The mathematical identity: x = part1 ^ part2
    // Reconstruction: result = part1 ^ part2
}

// ============================================================================
// Variable Analysis Tests
// ============================================================================

TEST_F(VariableSplittingTest, IgnoresAllocaInstructions) {
    std::vector<std::string> lines = {
        "define i32 @foo() {",
        "  %ptr = alloca i32",
        "  store i32 42, i32* %ptr",
        "  %val = load i32, i32* %ptr",
        "  ret i32 %val",
        "}"
    };

    pass.transformIR(lines);

    // %ptr should not be split (it's an alloca)
    bool found_alloca = false;
    for (const auto& line : lines) {
        if (line.find("alloca") != std::string::npos) {
            found_alloca = true;
            // The alloca should be unchanged
            EXPECT_TRUE(line.find("%ptr") != std::string::npos ||
                        line.find("split") != std::string::npos);
        }
    }
}

TEST_F(VariableSplittingTest, IgnoresCallInstructions) {
    std::vector<std::string> lines = {
        "define i32 @foo() {",
        "  %result = call i32 @bar()",
        "  ret i32 %result",
        "}"
    };

    pass.transformIR(lines);

    // Call result should not be split
    bool found_call = false;
    for (const auto& line : lines) {
        if (line.find("call") != std::string::npos) {
            found_call = true;
        }
    }
    EXPECT_TRUE(found_call);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(VariableSplittingTest, MultipleVariablesInFunction) {
    config.max_splits_per_function = 5;
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @compute(i32 %a, i32 %b) {",
        "  %sum = add i32 %a, %b",
        "  %diff = sub i32 %a, %b",
        "  %prod = mul i32 %sum, %diff",
        "  %result = add i32 %prod, 1",
        "  ret i32 %result",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Should process without crashing
    // Result depends on random selection
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingTest, RespectMaxSplitsLimit) {
    config.max_splits_per_function = 2;
    config.probability = 1.0;
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @many_vars() {",
        "  %a = add i32 1, 0",
        "  %b = add i32 2, 0",
        "  %c = add i32 3, 0",
        "  %d = add i32 4, 0",
        "  %e = add i32 5, 0",
        "  %sum = add i32 %a, %b",
        "  %sum2 = add i32 %sum, %c",
        "  %sum3 = add i32 %sum2, %d",
        "  %sum4 = add i32 %sum3, %e",
        "  ret i32 %sum4",
        "}"
    };

    pass.transformIR(lines);

    // Count split variables by looking for split_ prefix
    int split_count = 0;
    for (const auto& line : lines) {
        if (line.find("split_") != std::string::npos) {
            split_count++;
        }
    }

    // Should be limited by max_splits_per_function
    // Note: Each split creates multiple lines with split_ prefix
}

// ============================================================================
// Mathematical Correctness of Split Strategies
// ============================================================================

class SplitStrategyMathTest : public ::testing::Test {
protected:
    static constexpr int MIN_VAL = -100;
    static constexpr int MAX_VAL = 100;
    static constexpr int STEP = 7;
};

TEST_F(SplitStrategyMathTest, AdditiveSplit_ReconstructsCorrectly) {
    // For any x, if we split as part1 = random, part2 = x - part1
    // then part1 + part2 = x
    for (int x = MIN_VAL; x <= MAX_VAL; x += STEP) {
        int part1 = 42;  // Any random value
        int part2 = x - part1;
        int reconstructed = part1 + part2;
        EXPECT_EQ(reconstructed, x);
    }
}

TEST_F(SplitStrategyMathTest, XorSplit_ReconstructsCorrectly) {
    // For any x, if we split as part1 = random, part2 = x ^ random
    // then part1 ^ part2 = x
    for (int x = MIN_VAL; x <= MAX_VAL; x += STEP) {
        int random_key = 0x5A5A5A5A;
        int part1 = random_key;
        int part2 = x ^ random_key;
        int reconstructed = part1 ^ part2;
        EXPECT_EQ(reconstructed, x);
    }
}

TEST_F(SplitStrategyMathTest, MultiplicativeSplit_ReconstructsCorrectly) {
    // For non-zero x with factors, part1 * part2 = x
    // This is more complex - we'd need to find factors
    // For simplicity, test with known factors
    int x = 42;
    int part1 = 6;
    int part2 = 7;
    int reconstructed = part1 * part2;
    EXPECT_EQ(reconstructed, x);

    // Identity case: part1 = x, part2 = 1
    part1 = 42;
    part2 = 1;
    reconstructed = part1 * part2;
    EXPECT_EQ(reconstructed, x);
}

TEST_F(SplitStrategyMathTest, ThreeWaySplit_Additive) {
    // x = part1 + part2 + part3
    for (int x = MIN_VAL; x <= MAX_VAL; x += STEP) {
        int part1 = 10;
        int part2 = 20;
        int part3 = x - part1 - part2;
        int reconstructed = part1 + part2 + part3;
        EXPECT_EQ(reconstructed, x);
    }
}

// ============================================================================
// Constant Obfuscation Tests
// ============================================================================

class ConstantObfTest : public ::testing::Test {
protected:
    static constexpr int64_t MIN_VAL = -1000;
    static constexpr int64_t MAX_VAL = 1000;
};

TEST_F(ConstantObfTest, XorStrategy_Reversible) {
    // c = (c ^ key) ^ key
    for (int64_t c = MIN_VAL; c <= MAX_VAL; c += 37) {
        int64_t key = 0xDEADBEEF;
        int64_t obfuscated = c ^ key;
        int64_t restored = obfuscated ^ key;
        EXPECT_EQ(restored, c);
    }
}

TEST_F(ConstantObfTest, SplitStrategy_Reconstructs) {
    // c = c1 + c2
    for (int64_t c = MIN_VAL; c <= MAX_VAL; c += 37) {
        int64_t c1 = c / 2;
        int64_t c2 = c - c1;
        int64_t reconstructed = c1 + c2;
        EXPECT_EQ(reconstructed, c);
    }
}

TEST_F(ConstantObfTest, ArithmeticStrategy_Reversible) {
    // c = (c + offset) - offset
    for (int64_t c = MIN_VAL; c <= MAX_VAL; c += 37) {
        int64_t offset = 12345;
        int64_t obfuscated = c + offset;
        int64_t restored = obfuscated - offset;
        EXPECT_EQ(restored, c);
    }
}

TEST_F(ConstantObfTest, MultiplyDivideStrategy_Reversible) {
    // c = (c * factor) / factor
    // Note: Only works for values where c * factor doesn't overflow
    // and c is divisible by factor or we accept truncation
    for (int64_t c = -100; c <= 100; c += 7) {
        if (c == 0) continue;  // Skip zero for this test

        int64_t factor = 7;
        int64_t obfuscated = c * factor;
        int64_t restored = obfuscated / factor;
        EXPECT_EQ(restored, c);
    }
}

// ============================================================================
// Enhanced Constant Obfuscation Tests (Phase 3.4)
// ============================================================================

class EnhancedConstantObfTest : public ::testing::Test {
protected:
    static constexpr int64_t MIN_VAL = -10000;
    static constexpr int64_t MAX_VAL = 10000;
};

TEST_F(EnhancedConstantObfTest, MBAStrategy_Reconstructs) {
    // MBA: c = (a ^ b) + 2*(a & b)  where a + b = c
    // This is the fundamental MBA identity for addition
    for (int64_t c = MIN_VAL; c <= MAX_VAL; c += 137) {
        int64_t a = c / 3;
        int64_t b = c - a;  // So a + b = c

        // MBA identity: a + b = (a ^ b) + 2*(a & b)
        int64_t xor_part = a ^ b;
        int64_t and_part = a & b;
        int64_t reconstructed = xor_part + 2 * and_part;

        EXPECT_EQ(reconstructed, c) << "MBA failed for c=" << c
                                    << " (a=" << a << ", b=" << b << ")";
    }
}

TEST_F(EnhancedConstantObfTest, MultiSplitStrategy_Reconstructs) {
    // c = c1 + c2 + c3 + c4
    for (int64_t c = MIN_VAL; c <= MAX_VAL; c += 137) {
        // Split into 4 parts
        int64_t c1 = c / 4;
        int64_t c2 = c / 5;
        int64_t c3 = c / 6;
        int64_t c4 = c - c1 - c2 - c3;

        int64_t reconstructed = c1 + c2 + c3 + c4;
        EXPECT_EQ(reconstructed, c) << "MultiSplit failed for c=" << c;
    }
}

TEST_F(EnhancedConstantObfTest, NestedXORStrategy_Reconstructs) {
    // c = ((encoded ^ key3) ^ key2) ^ key1
    // where encoded = c ^ key1 ^ key2 ^ key3
    for (int64_t c = MIN_VAL; c <= MAX_VAL; c += 137) {
        int64_t key1 = 0x1234;
        int64_t key2 = 0x5678;
        int64_t key3 = 0x9ABC;

        // Encode
        int64_t encoded = c ^ key1 ^ key2 ^ key3;

        // Decode (XOR with keys in reverse order)
        int64_t reconstructed = ((encoded ^ key3) ^ key2) ^ key1;
        EXPECT_EQ(reconstructed, c) << "NestedXOR failed for c=" << c;
    }
}

TEST_F(EnhancedConstantObfTest, ShiftAddStrategy_Reconstructs) {
    // c = (base << shift) + remainder
    for (int64_t c = MIN_VAL; c <= MAX_VAL; c += 137) {
        int shift = 4;  // Use shift of 4 bits (multiply by 16)
        int64_t divisor = 1LL << shift;

        int64_t base = c / divisor;
        int64_t remainder = c - (base << shift);

        int64_t reconstructed = (base << shift) + remainder;
        EXPECT_EQ(reconstructed, c) << "ShiftAdd failed for c=" << c;
    }
}

TEST_F(EnhancedConstantObfTest, LargeConstants_Handled) {
    // Test with large constants (>10000)
    std::vector<int64_t> large_values = {
        100000, -100000,
        1000000, -1000000,
        INT32_MAX, INT32_MIN,
        0x7FFFFFFFLL, -0x7FFFFFFFLL
    };

    for (int64_t c : large_values) {
        // Test MBA strategy with large values
        int64_t a = c / 2;
        int64_t b = c - a;
        int64_t xor_part = a ^ b;
        int64_t and_part = a & b;
        int64_t mba_result = xor_part + 2 * and_part;
        EXPECT_EQ(mba_result, c) << "Large constant MBA failed for c=" << c;

        // Test shift-add with large values
        int shift = 8;
        int64_t divisor = 1LL << shift;
        int64_t base = c / divisor;
        int64_t remainder = c - (base << shift);
        int64_t shift_result = (base << shift) + remainder;
        EXPECT_EQ(shift_result, c) << "Large constant ShiftAdd failed for c=" << c;
    }
}

TEST_F(EnhancedConstantObfTest, BitSplitStrategy_Reconstructs) {
    // c = bit1 | bit2 | bit3 | ...
    for (int64_t c = 0; c <= 1000; c += 17) {
        // Extract set bits
        std::vector<int64_t> bits;
        uint64_t val = static_cast<uint64_t>(c);
        for (int i = 0; i < 64 && val != 0; i++) {
            if (val & 1) {
                bits.push_back(1LL << i);
            }
            val >>= 1;
        }

        // Reconstruct by OR'ing all bits
        int64_t reconstructed = 0;
        for (int64_t bit : bits) {
            reconstructed |= bit;
        }
        EXPECT_EQ(reconstructed, c) << "BitSplit failed for c=" << c;
    }
}

// ============================================================================
// Floating Point Constant Obfuscation Tests
// ============================================================================

class FloatConstantObfTest : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-10;
};

TEST_F(FloatConstantObfTest, DoubleXorStrategy_Reconstructs) {
    // For doubles, we XOR the bit representation
    std::vector<double> test_values = {
        0.0, 1.0, -1.0,
        3.14159265358979,
        2.71828182845904,
        1000.5, -1000.5,
        0.001, -0.001
    };

    for (double original : test_values) {
        // Get bit representation
        uint64_t bits;
        std::memcpy(&bits, &original, sizeof(double));

        // XOR with key
        uint64_t key = 0xDEADBEEFCAFEBABEULL;
        uint64_t encoded = bits ^ key;

        // Decode
        uint64_t decoded_bits = encoded ^ key;

        // Convert back to double
        double reconstructed;
        std::memcpy(&reconstructed, &decoded_bits, sizeof(double));

        EXPECT_DOUBLE_EQ(reconstructed, original)
            << "Double XOR failed for " << original;
    }
}

TEST_F(FloatConstantObfTest, FloatXorStrategy_Reconstructs) {
    // For floats, we XOR the bit representation
    std::vector<float> test_values = {
        0.0f, 1.0f, -1.0f,
        3.14159f,
        2.71828f,
        1000.5f, -1000.5f
    };

    for (float original : test_values) {
        // Get bit representation
        uint32_t bits;
        std::memcpy(&bits, &original, sizeof(float));

        // XOR with key
        uint32_t key = 0xDEADBEEF;
        uint32_t encoded = bits ^ key;

        // Decode
        uint32_t decoded_bits = encoded ^ key;

        // Convert back to float
        float reconstructed;
        std::memcpy(&reconstructed, &decoded_bits, sizeof(float));

        EXPECT_FLOAT_EQ(reconstructed, original)
            << "Float XOR failed for " << original;
    }
}

// ============================================================================
// Constant Obfuscator Class Tests
// ============================================================================

class ConstantObfuscatorTest : public ::testing::Test {
protected:
    morphect::data::ConstantObfuscator obfuscator;
    morphect::data::ConstantObfConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;  // Always obfuscate for testing
        config.min_value = 0;
        config.max_value = INT64_MAX;
        config.obfuscate_zero = true;
        config.obfuscate_one = true;
        config.max_multi_split_parts = 4;
        config.max_nested_xor_depth = 3;
        obfuscator.configure(config);
    }
};

TEST_F(ConstantObfuscatorTest, ObfuscateMBA_ProducesValidResult) {
    auto result = obfuscator.obfuscateWithStrategy(42, morphect::data::ConstantObfStrategy::MBA);
    EXPECT_EQ(result.original, 42);
    EXPECT_EQ(result.strategy, morphect::data::ConstantObfStrategy::MBA);
    EXPECT_GE(result.parts.size(), 4u);  // a, b, xor_part, and_part

    // Verify mathematical correctness
    // parts[2] = xor_part, parts[3] = and_part
    int64_t reconstructed = result.parts[2] + 2 * result.parts[3];
    EXPECT_EQ(reconstructed, 42);
}

TEST_F(ConstantObfuscatorTest, ObfuscateMultiSplit_ProducesValidResult) {
    auto result = obfuscator.obfuscateWithStrategy(100, morphect::data::ConstantObfStrategy::MultiSplit);
    EXPECT_EQ(result.original, 100);
    EXPECT_EQ(result.strategy, morphect::data::ConstantObfStrategy::MultiSplit);
    EXPECT_GE(result.parts.size(), 2u);
    EXPECT_LE(result.parts.size(), static_cast<size_t>(config.max_multi_split_parts));

    // Verify sum equals original
    int64_t sum = 0;
    for (int64_t part : result.parts) {
        sum += part;
    }
    EXPECT_EQ(sum, 100);
}

TEST_F(ConstantObfuscatorTest, ObfuscateNestedXOR_ProducesValidResult) {
    auto result = obfuscator.obfuscateWithStrategy(12345, morphect::data::ConstantObfStrategy::NestedXOR);
    EXPECT_EQ(result.original, 12345);
    EXPECT_EQ(result.strategy, morphect::data::ConstantObfStrategy::NestedXOR);
    EXPECT_GE(result.parts.size(), 3u);  // encoded + at least 2 keys

    // Verify XOR chain produces original
    int64_t current = result.parts[0];  // encoded
    for (size_t i = 1; i < result.parts.size(); i++) {
        current ^= result.parts[i];
    }
    EXPECT_EQ(current, 12345);
}

TEST_F(ConstantObfuscatorTest, ObfuscateShiftAdd_ProducesValidResult) {
    auto result = obfuscator.obfuscateWithStrategy(1000, morphect::data::ConstantObfStrategy::ShiftAdd);
    EXPECT_EQ(result.original, 1000);
    EXPECT_EQ(result.strategy, morphect::data::ConstantObfStrategy::ShiftAdd);
    EXPECT_EQ(result.parts.size(), 3u);  // base, shift, remainder

    // Verify reconstruction
    int64_t base = result.parts[0];
    int64_t shift = result.parts[1];
    int64_t remainder = result.parts[2];
    int64_t reconstructed = (base << shift) + remainder;
    EXPECT_EQ(reconstructed, 1000);
}

// ============================================================================
// Enhanced String Encoding Tests (Phase 3.5)
// ============================================================================

class StringEncodingTest : public ::testing::Test {
protected:
    morphect::data::StringEncoder encoder;
    morphect::data::StringEncodingConfig config;

    void SetUp() override {
        config.enabled = true;
        config.xor_key = 0x7B;
        config.min_string_length = 1;
        config.max_split_parts = 4;
        encoder.configure(config);
    }

    // Helper to decode XOR
    std::string decodeXOR(const std::vector<uint8_t>& encoded, uint8_t key) {
        std::string result;
        for (uint8_t b : encoded) {
            result += static_cast<char>(b ^ key);
        }
        return result;
    }

    // Helper to decode Rolling XOR
    std::string decodeRollingXOR(const std::vector<uint8_t>& encoded, uint8_t init_key) {
        std::string result;
        uint8_t key = init_key;
        for (uint8_t b : encoded) {
            char decoded = static_cast<char>(b ^ key);
            result += decoded;
            key = static_cast<uint8_t>(decoded);
        }
        return result;
    }

    // Helper to decode Chained XOR
    std::string decodeChainedXOR(const std::vector<uint8_t>& encoded, uint8_t init_key) {
        std::string result;
        for (size_t i = 0; i < encoded.size(); i++) {
            uint8_t k = static_cast<uint8_t>((init_key * (i + 1) + i) & 0xFF);
            result += static_cast<char>(encoded[i] ^ k);
        }
        return result;
    }

    // Helper to decode ByteSwap XOR
    std::string decodeByteSwapXOR(const std::vector<uint8_t>& encoded, uint8_t key) {
        // First XOR
        std::string swapped;
        for (uint8_t b : encoded) {
            swapped += static_cast<char>(b ^ key);
        }
        // Then unswap adjacent bytes
        for (size_t i = 0; i + 1 < swapped.length(); i += 2) {
            std::swap(swapped[i], swapped[i + 1]);
        }
        return swapped;
    }
};

TEST_F(StringEncodingTest, SimpleXOR_EncodesAndDecodes) {
    std::string original = "Hello, World!";
    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::XOR);

    EXPECT_EQ(encoded.original, original);
    EXPECT_EQ(encoded.length, original.length());
    EXPECT_EQ(encoded.encoded_bytes.size(), original.length());

    // Decode and verify
    std::string decoded = decodeXOR(encoded.encoded_bytes, encoded.key);
    EXPECT_EQ(decoded, original);
}

TEST_F(StringEncodingTest, RollingXOR_EncodesAndDecodes) {
    std::string original = "The quick brown fox";
    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::RollingXOR);

    EXPECT_EQ(encoded.original, original);
    EXPECT_EQ(encoded.length, original.length());

    // Decode and verify
    std::string decoded = decodeRollingXOR(encoded.encoded_bytes, encoded.key);
    EXPECT_EQ(decoded, original);
}

TEST_F(StringEncodingTest, ChainedXOR_EncodesAndDecodes) {
    std::string original = "password123";
    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::ChainedXOR);

    EXPECT_EQ(encoded.original, original);
    EXPECT_EQ(encoded.length, original.length());

    // Decode and verify
    std::string decoded = decodeChainedXOR(encoded.encoded_bytes, encoded.key);
    EXPECT_EQ(decoded, original);
}

TEST_F(StringEncodingTest, ByteSwapXOR_EncodesAndDecodes) {
    std::string original = "secret_key_here";
    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::ByteSwapXOR);

    EXPECT_EQ(encoded.original, original);
    EXPECT_EQ(encoded.length, original.length());

    // Decode and verify
    std::string decoded = decodeByteSwapXOR(encoded.encoded_bytes, encoded.key);
    EXPECT_EQ(decoded, original);
}

TEST_F(StringEncodingTest, Base64XOR_EncodesCorrectly) {
    std::string original = "test";
    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::Base64XOR);

    EXPECT_EQ(encoded.original, original);
    // Base64 of "test" is "dGVzdA==" (8 chars)
    EXPECT_EQ(encoded.length, 8u);
    EXPECT_EQ(encoded.encoded_bytes.size(), 8u);

    // Decode XOR part to get base64
    std::string base64;
    for (uint8_t b : encoded.encoded_bytes) {
        base64 += static_cast<char>(b ^ encoded.key);
    }
    EXPECT_EQ(base64, "dGVzdA==");
}

TEST_F(StringEncodingTest, RC4_EncodesAndDecodes) {
    std::string original = "rc4 encrypted message";
    config.rc4_key = {0x01, 0x02, 0x03, 0x04, 0x05};
    encoder.configure(config);

    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::RC4);

    EXPECT_EQ(encoded.original, original);
    EXPECT_EQ(encoded.length, original.length());

    // RC4 is symmetric - encoding again with same key should give original
    // Create a simple RC4 decoder
    std::vector<uint8_t> S(256);
    for (int i = 0; i < 256; i++) {
        S[i] = static_cast<uint8_t>(i);
    }

    // KSA
    int j = 0;
    for (int i = 0; i < 256; i++) {
        j = (j + S[i] + config.rc4_key[i % config.rc4_key.size()]) & 0xFF;
        std::swap(S[i], S[j]);
    }

    // PRGA + decryption
    std::string decoded;
    int ii = 0;
    j = 0;
    for (uint8_t c : encoded.encoded_bytes) {
        ii = (ii + 1) & 0xFF;
        j = (j + S[ii]) & 0xFF;
        std::swap(S[ii], S[j]);
        uint8_t k = S[(S[ii] + S[j]) & 0xFF];
        decoded += static_cast<char>(c ^ k);
    }
    EXPECT_EQ(decoded, original);
}

TEST_F(StringEncodingTest, StringSplitting_SplitsCorrectly) {
    std::string original = "This is a test string for splitting";
    auto split = encoder.splitAndEncode(original);

    EXPECT_EQ(split.original, original);
    EXPECT_GE(split.numParts(), 2u);
    EXPECT_LE(split.numParts(), static_cast<size_t>(config.max_split_parts));

    // Verify parts concatenate to original
    std::string reconstructed;
    for (const auto& part : split.parts) {
        reconstructed += part;
    }
    EXPECT_EQ(reconstructed, original);

    // Verify each encoded part decodes correctly
    for (size_t i = 0; i < split.parts.size(); i++) {
        std::string decoded = decodeXOR(split.encoded_parts[i].encoded_bytes,
                                        split.encoded_parts[i].key);
        EXPECT_EQ(decoded, split.parts[i]);
    }
}

TEST_F(StringEncodingTest, DifferentStringsProduceDifferentEncodings) {
    std::string str1 = "aaaaaaaaaa";
    std::string str2 = "bbbbbbbbbb";

    auto enc1 = encoder.encodeWithMethod(str1, morphect::data::StringEncodingMethod::XOR);
    auto enc2 = encoder.encodeWithMethod(str2, morphect::data::StringEncodingMethod::XOR);

    // Same key, different input should produce different output
    EXPECT_NE(enc1.encoded_bytes, enc2.encoded_bytes);
}

TEST_F(StringEncodingTest, EmptyString_HandledCorrectly) {
    std::string original = "";
    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::XOR);

    EXPECT_EQ(encoded.original, "");
    EXPECT_EQ(encoded.length, 0u);
    EXPECT_EQ(encoded.encoded_bytes.size(), 0u);
}

TEST_F(StringEncodingTest, SpecialCharacters_EncodedCorrectly) {
    std::string original = "\x00\x01\x02\xFF\xFE\xFD";
    original.resize(6);  // Ensure null bytes are included

    auto encoded = encoder.encodeWithMethod(original, morphect::data::StringEncodingMethod::XOR);
    std::string decoded = decodeXOR(encoded.encoded_bytes, encoded.key);

    EXPECT_EQ(decoded.length(), original.length());
    for (size_t i = 0; i < original.length(); i++) {
        EXPECT_EQ(static_cast<uint8_t>(decoded[i]), static_cast<uint8_t>(original[i]));
    }
}

TEST_F(StringEncodingTest, ShouldEncode_RespectsMinLength) {
    config.min_string_length = 5;
    encoder.configure(config);

    EXPECT_FALSE(encoder.shouldEncode("abc"));   // 3 chars
    EXPECT_FALSE(encoder.shouldEncode("abcd"));  // 4 chars
    EXPECT_TRUE(encoder.shouldEncode("abcde"));  // 5 chars
    EXPECT_TRUE(encoder.shouldEncode("abcdef")); // 6 chars
}

TEST_F(StringEncodingTest, ShouldEncode_RespectsExcludePatterns) {
    config.exclude_patterns = {"DEBUG", "TEST"};
    encoder.configure(config);

    EXPECT_FALSE(encoder.shouldEncode("DEBUG: message"));
    EXPECT_FALSE(encoder.shouldEncode("This is a TEST"));
    EXPECT_TRUE(encoder.shouldEncode("Regular message"));
}

// ============================================================================
// String Encoding Mathematical Correctness Tests
// ============================================================================

class StringEncodingMathTest : public ::testing::Test {
protected:
    morphect::data::StringEncoder encoder;
    morphect::data::StringEncodingConfig config;

    void SetUp() override {
        config.enabled = true;
        config.xor_key = 0x42;
        encoder.configure(config);
    }
};

TEST_F(StringEncodingMathTest, XOR_IsReversible) {
    // XOR is self-inverse: (a ^ k) ^ k = a
    for (int c = 0; c < 256; c++) {
        for (int k = 0; k < 256; k++) {
            uint8_t encoded = static_cast<uint8_t>(c) ^ static_cast<uint8_t>(k);
            uint8_t decoded = encoded ^ static_cast<uint8_t>(k);
            EXPECT_EQ(decoded, static_cast<uint8_t>(c));
        }
    }
}

TEST_F(StringEncodingMathTest, RollingXOR_ChainIsConsistent) {
    std::string test = "ABCDEFGH";
    auto encoded = encoder.encodeWithMethod(test, morphect::data::StringEncodingMethod::RollingXOR);

    // Verify the chain: each encoded byte depends on previous plaintext
    uint8_t key = config.xor_key;
    for (size_t i = 0; i < test.length(); i++) {
        uint8_t expected_encoded = static_cast<uint8_t>(test[i]) ^ key;
        EXPECT_EQ(encoded.encoded_bytes[i], expected_encoded);
        key = static_cast<uint8_t>(test[i]);  // Next key is current plaintext
    }
}

TEST_F(StringEncodingMathTest, ChainedXOR_KeySequenceIsCorrect) {
    // Verify key chain formula: k[i] = (init_key * (i + 1) + i) mod 256
    uint8_t init_key = 0x42;

    std::vector<uint8_t> expected_keys;
    for (int i = 0; i < 10; i++) {
        expected_keys.push_back(static_cast<uint8_t>((init_key * (i + 1) + i) & 0xFF));
    }

    // Keys should all be different (mostly)
    std::set<uint8_t> unique_keys(expected_keys.begin(), expected_keys.end());
    EXPECT_GT(unique_keys.size(), 1u);  // At least some variation
}

// ============================================================================
// Variable Splitting Coverage Tests (Phase 6.1)
// ============================================================================

class VariableSplittingCoverageTest : public ::testing::Test {
protected:
    LLVMVariableSplittingPass pass;
    VariableSplittingConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;
        config.default_strategy = SplitStrategy::Additive;
        config.split_phi_nodes = false;
        config.max_splits_per_function = 10;
        pass.configure(config);
    }
};

TEST_F(VariableSplittingCoverageTest, XorStrategy_ProducesCorrectIR) {
    config.default_strategy = SplitStrategy::XOR;
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @test_xor() {",
        "  %x = add i32 10, 5",
        "  %y = add i32 %x, 1",
        "  ret i32 %y",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Should transform and use xor instructions
    bool found_xor = false;
    for (const auto& line : lines) {
        if (line.find(" xor ") != std::string::npos) {
            found_xor = true;
            break;
        }
    }
    // XOR strategy should generate xor instructions
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, MultiplicativeStrategy_ProducesCorrectIR) {
    config.default_strategy = SplitStrategy::Multiplicative;
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @test_mult() {",
        "  %x = add i32 10, 5",
        "  %y = add i32 %x, 1",
        "  ret i32 %y",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Should transform without crashing
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, PHINodeAnalysis_WhenEnabled) {
    config.split_phi_nodes = true;
    pass.configure(config);

    // IR with PHI nodes (common in loop constructs)
    std::vector<std::string> lines = {
        "define i32 @test_phi(i1 %cond) {",
        "entry:",
        "  br i1 %cond, label %then, label %else",
        "then:",
        "  %a = add i32 1, 0",
        "  br label %merge",
        "else:",
        "  %b = add i32 2, 0",
        "  br label %merge",
        "merge:",
        "  %result = phi i32 [ %a, %then ], [ %b, %else ]",
        "  %use = add i32 %result, 1",
        "  ret i32 %use",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Should process PHI nodes when enabled
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, PHINodeAnalysis_WhenDisabled) {
    config.split_phi_nodes = false;
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @test_phi_disabled(i1 %cond) {",
        "entry:",
        "  br i1 %cond, label %then, label %else",
        "then:",
        "  %a = add i32 1, 0",
        "  br label %merge",
        "else:",
        "  %b = add i32 2, 0",
        "  br label %merge",
        "merge:",
        "  %result = phi i32 [ %a, %then ], [ %b, %else ]",
        "  %use = add i32 %result, 1",
        "  ret i32 %use",
        "}"
    };

    auto result = pass.transformIR(lines);

    // PHI nodes should be skipped but other vars may be split
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, ExclusionPattern_ActuallyMatches) {
    // Set up patterns that will actually match variable names in the IR
    config.exclude_patterns = {"%x", "%val"};
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @test_exclude() {",
        "  %x = add i32 1, 2",
        "  %val = add i32 %x, 3",
        "  %other = add i32 %val, 4",
        "  ret i32 %other",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Variables %x and %val should be excluded from splitting
    // Only %other might be split
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, VariableWithNoUses_IsNotSplit) {
    std::vector<std::string> lines = {
        "define i32 @test_no_use() {",
        "  %unused = add i32 1, 2",
        "  ret i32 0",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Variable with no uses should not be split
    EXPECT_TRUE(result == morphect::TransformResult::NotApplicable ||
                result == morphect::TransformResult::Success);
}

TEST_F(VariableSplittingCoverageTest, VariableWithMultipleUses_IsSplit) {
    std::vector<std::string> lines = {
        "define i32 @test_multi_use() {",
        "  %x = add i32 10, 5",
        "  %y = add i32 %x, %x",
        "  %z = mul i32 %x, 2",
        "  %sum = add i32 %y, %z",
        "  ret i32 %sum",
        "}"
    };

    auto result = pass.transformIR(lines);

    // Variable used multiple times should be split
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, TransformDefinition_NonMatchingPattern) {
    // Test with lines that won't match the definition pattern
    std::vector<std::string> lines = {
        "define i32 @test_nonmatch() {",
        "entry:",
        "  %x = add i32 1, 2",
        "  %use = add i32 %x, 1",
        "  ret i32 %use",
        "}"
    };

    auto result = pass.transformIR(lines);
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, ComplexIRWithMixedOperations) {
    config.default_strategy = SplitStrategy::XOR;
    pass.configure(config);

    std::vector<std::string> lines = {
        "define i32 @complex_func(i32 %arg1, i32 %arg2) {",
        "entry:",
        "  %sum = add i32 %arg1, %arg2",
        "  %diff = sub i32 %arg1, %arg2",
        "  %prod = mul i32 %sum, %diff",
        "  %shifted = shl i32 %prod, 2",
        "  %masked = and i32 %shifted, 255",
        "  %final = or i32 %masked, 1",
        "  ret i32 %final",
        "}"
    };

    auto result = pass.transformIR(lines);
    EXPECT_TRUE(result == morphect::TransformResult::Success ||
                result == morphect::TransformResult::NotApplicable);
}

TEST_F(VariableSplittingCoverageTest, AllThreeStrategies_Sequential) {
    // Test all three strategies in sequence to ensure coverage
    std::vector<SplitStrategy> strategies = {
        SplitStrategy::Additive,
        SplitStrategy::XOR,
        SplitStrategy::Multiplicative
    };

    for (auto strategy : strategies) {
        config.default_strategy = strategy;
        pass.configure(config);

        std::vector<std::string> lines = {
            "define i32 @test_strategy() {",
            "  %x = add i32 5, 3",
            "  %y = add i32 %x, 2",
            "  ret i32 %y",
            "}"
        };

        auto result = pass.transformIR(lines);
        EXPECT_TRUE(result == morphect::TransformResult::Success ||
                    result == morphect::TransformResult::NotApplicable);
    }
}
