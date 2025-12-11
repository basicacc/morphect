/**
 * Morphect - Obfuscation Verification Tests
 *
 * These tests verify that obfuscation transformations actually occur,
 * not just that the output produces correct results.
 *
 * Tests check:
 * 1. MBA: Original arithmetic instructions are replaced with MBA patterns
 * 2. CFF: Control flow is actually flattened (dispatcher/switch present)
 * 3. Dead Code: Additional instructions are inserted
 */

#include <gtest/gtest.h>
#include "../fixtures/obfuscation_fixture.hpp"
#include "passes/mba/mba.hpp"
#include "passes/cff/cff.hpp"
#include "passes/deadcode/deadcode.hpp"

using namespace morphect::test;
using namespace morphect;

// ============================================================================
// MBA Obfuscation Verification Tests
// ============================================================================

class MBAVerificationTest : public LLVMIRFixture {
protected:
    mba::LLVMMBAPass mba_pass;
    mba::MBAPassConfig config;

    void SetUp() override {
        LLVMIRFixture::SetUp();

        config.global.enabled = true;
        config.global.probability = 1.0;  // Always transform
        config.global.nesting_depth = 1;
        config.enable_add = true;
        config.enable_sub = true;
        config.enable_xor = true;
        config.enable_and = true;
        config.enable_or = true;
        mba_pass.initializeMBA(config);
    }

    int countInstruction(const std::vector<std::string>& lines, const std::string& instr) {
        int count = 0;
        for (const auto& line : lines) {
            // Look for instruction pattern: "= instr "
            std::string pattern = "= " + instr + " ";
            if (line.find(pattern) != std::string::npos) {
                count++;
            }
        }
        return count;
    }
};

TEST_F(MBAVerificationTest, AddActuallyTransformed) {
    // Simple add instruction
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %result = add i32 %a, %b",
        "  ret i32 %result",
        "}"
    };

    int original_add_count = countInstruction(lines, "add");
    ASSERT_EQ(original_add_count, 1) << "Should have exactly 1 add before transformation";

    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success) << "MBA transformation should succeed";

    // After MBA, the original "add i32 %a, %b" should be gone or expanded
    // MBA replaces add with combinations of xor, and, or, sub, etc.
    int post_add_count = 0;
    for (const auto& line : lines) {
        // Count original-style adds (add i32 %a, %b)
        if (line.find("add i32 %a, %b") != std::string::npos) {
            post_add_count++;
        }
    }

    // The original add should be replaced
    EXPECT_EQ(post_add_count, 0) << "Original add instruction should be transformed";

    // Should now have some MBA operations - ADD has 6 variants:
    // Variants 0,1,2,3 use xor/and/or
    // Variant 4 uses sub (a - (-b))
    // Variant 5 uses xor with -1 for NOT (~(~a - b))
    // All variants produce either xor, and, or, or sub operations
    bool has_mba_ops = false;
    for (const auto& line : lines) {
        if (line.find("= xor ") != std::string::npos ||
            line.find("= and ") != std::string::npos ||
            line.find("= or ") != std::string::npos ||
            line.find("= sub ") != std::string::npos ||
            line.find("= shl ") != std::string::npos) {
            has_mba_ops = true;
            break;
        }
    }
    EXPECT_TRUE(has_mba_ops) << "Should have MBA operations after transformation";

    // Should have more lines after transformation
    EXPECT_GT(lines.size(), 5u) << "Transformed code should be longer";
}

TEST_F(MBAVerificationTest, XorActuallyTransformed) {
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %result = xor i32 %a, %b",
        "  ret i32 %result",
        "}"
    };

    int original_xor_count = 0;
    for (const auto& line : lines) {
        if (line.find("xor i32 %a, %b") != std::string::npos) {
            original_xor_count++;
        }
    }
    ASSERT_EQ(original_xor_count, 1);

    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success);

    // Original xor should be transformed
    int post_xor_count = 0;
    for (const auto& line : lines) {
        if (line.find("xor i32 %a, %b") != std::string::npos) {
            post_xor_count++;
        }
    }
    EXPECT_EQ(post_xor_count, 0) << "Original xor should be transformed";

    // Should have MBA operations
    bool has_or = false, has_and = false;
    for (const auto& line : lines) {
        if (line.find("= or ") != std::string::npos) has_or = true;
        if (line.find("= and ") != std::string::npos) has_and = true;
    }
    // XOR MBA typically uses OR and AND
    EXPECT_TRUE(has_or || has_and) << "XOR MBA should introduce OR or AND";
}

TEST_F(MBAVerificationTest, AndActuallyTransformed) {
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %result = and i32 %a, %b",
        "  ret i32 %result",
        "}"
    };

    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success);

    // Check transformation happened
    int simple_and_count = 0;
    for (const auto& line : lines) {
        if (line.find("and i32 %a, %b") != std::string::npos) {
            simple_and_count++;
        }
    }
    EXPECT_EQ(simple_and_count, 0) << "Original and should be transformed";

    // Should have MBA operations (or, xor, sub for AND transformation)
    bool has_or = false, has_xor = false;
    for (const auto& line : lines) {
        if (line.find("= or ") != std::string::npos) has_or = true;
        if (line.find("= xor ") != std::string::npos) has_xor = true;
    }
    EXPECT_TRUE(has_or || has_xor) << "AND MBA should introduce OR or XOR";
}

TEST_F(MBAVerificationTest, OrActuallyTransformed) {
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %result = or i32 %a, %b",
        "  ret i32 %result",
        "}"
    };

    size_t original_size = lines.size();

    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success);

    // Check that transformation happened - lines should increase
    // OR MBA has 6 variants, all add at least 2 instructions
    EXPECT_GT(lines.size(), original_size)
        << "MBA should add instructions";

    // OR MBA introduces xor and/or and operations
    // Note: Variant 5 produces "(a & b) | (a ^ b)" which still uses OR
    // but with different operands (temp vars instead of %a, %b)
    bool has_xor = false, has_and = false;
    for (const auto& line : lines) {
        if (line.find("= xor ") != std::string::npos) has_xor = true;
        if (line.find("= and ") != std::string::npos) has_and = true;
    }
    EXPECT_TRUE(has_xor || has_and) << "OR MBA should introduce XOR or AND";
}

TEST_F(MBAVerificationTest, SubActuallyTransformed) {
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %result = sub i32 %a, %b",
        "  ret i32 %result",
        "}"
    };

    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success);

    int simple_sub_count = 0;
    for (const auto& line : lines) {
        if (line.find("sub i32 %a, %b") != std::string::npos) {
            simple_sub_count++;
        }
    }
    EXPECT_EQ(simple_sub_count, 0) << "Original sub should be transformed";

    // SUB MBA uses xor, and, add
    bool has_xor = false, has_and = false;
    for (const auto& line : lines) {
        if (line.find("= xor ") != std::string::npos) has_xor = true;
        if (line.find("= and ") != std::string::npos) has_and = true;
    }
    EXPECT_TRUE(has_xor || has_and) << "SUB MBA should introduce XOR or AND";
}

TEST_F(MBAVerificationTest, MultipleOpsTransformed) {
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b, i32 %c) {",
        "entry:",
        "  %t1 = add i32 %a, %b",
        "  %t2 = xor i32 %t1, %c",
        "  %t3 = and i32 %t2, %a",
        "  ret i32 %t3",
        "}"
    };

    size_t original_lines = lines.size();
    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success);

    // Should have significantly more lines after transformation
    EXPECT_GT(lines.size(), original_lines + 5)
        << "Multiple MBA transformations should add many lines";

    // Count MBA operations
    int mba_ops = 0;
    for (const auto& line : lines) {
        if (line.find("= xor ") != std::string::npos ||
            line.find("= and ") != std::string::npos ||
            line.find("= or ") != std::string::npos) {
            mba_ops++;
        }
    }
    EXPECT_GE(mba_ops, 3) << "Should have multiple MBA operations";
}

// ============================================================================
// CFF Obfuscation Verification Tests
// ============================================================================

class CFFVerificationTest : public ::testing::Test {
protected:
    cff::LLVMCFGAnalyzer analyzer;
    cff::LLVMCFFTransformation transformer;
    cff::CFFConfig config;

    void SetUp() override {
        config.enabled = true;
        config.probability = 1.0;
        config.min_blocks = 2;
        config.shuffle_states = true;
    }
};

TEST_F(CFFVerificationTest, FlatteningCreatesDispatcher) {
    std::vector<std::string> ir = {
        "define i32 @test(i32 %a) {",
        "entry:",
        "  %cmp = icmp sgt i32 %a, 0",
        "  br i1 %cmp, label %then, label %else",
        "then:",
        "  %t1 = add i32 %a, 1",
        "  br label %end",
        "else:",
        "  %t2 = sub i32 %a, 1",
        "  br label %end",
        "end:",
        "  %result = phi i32 [ %t1, %then ], [ %t2, %else ]",
        "  ret i32 %result",
        "}"
    };

    auto cfg_opt = analyzer.analyze(ir);
    ASSERT_TRUE(cfg_opt.has_value()) << "CFG analysis should succeed";

    auto result = transformer.flatten(cfg_opt.value(), config);
    ASSERT_TRUE(result.success) << "Flattening should succeed: " << result.error;

    // Check for dispatcher block
    bool has_dispatcher = false;
    bool has_switch = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("dispatcher:") != std::string::npos ||
            line.find("switch_block:") != std::string::npos) {
            has_dispatcher = true;
        }
        if (line.find("switch ") != std::string::npos) {
            has_switch = true;
        }
    }

    EXPECT_TRUE(has_dispatcher) << "Flattened code should have dispatcher block";
    EXPECT_TRUE(has_switch) << "Flattened code should have switch statement";
}

TEST_F(CFFVerificationTest, FlatteningCreatesStateVariable) {
    std::vector<std::string> ir = {
        "define i32 @test(i32 %a) {",
        "entry:",
        "  br label %b1",
        "b1:",
        "  %t1 = add i32 %a, 1",
        "  br label %b2",
        "b2:",
        "  ret i32 %t1",
        "}"
    };

    auto cfg_opt = analyzer.analyze(ir);
    ASSERT_TRUE(cfg_opt.has_value());

    auto result = transformer.flatten(cfg_opt.value(), config);
    ASSERT_TRUE(result.success) << "Flattening should succeed: " << result.error;

    // Check for state variable
    bool has_state_var = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("_cff_state") != std::string::npos ||
            line.find("state") != std::string::npos) {
            has_state_var = true;
            break;
        }
    }

    EXPECT_TRUE(has_state_var) << "Flattened code should have state variable";
    EXPECT_GT(result.states_created, 0) << "Should have created states";
}

TEST_F(CFFVerificationTest, FlatteningIncreasesBlockCount) {
    std::vector<std::string> ir = {
        "define i32 @test(i32 %n) {",
        "entry:",
        "  %cmp = icmp sgt i32 %n, 0",
        "  br i1 %cmp, label %positive, label %negative",
        "positive:",
        "  %p = mul i32 %n, 2",
        "  br label %done",
        "negative:",
        "  %q = sub i32 0, %n",
        "  br label %done",
        "done:",
        "  %result = phi i32 [ %p, %positive ], [ %q, %negative ]",
        "  ret i32 %result",
        "}"
    };

    auto cfg_opt = analyzer.analyze(ir);
    ASSERT_TRUE(cfg_opt.has_value());

    int original_blocks = cfg_opt.value().num_blocks;

    auto result = transformer.flatten(cfg_opt.value(), config);
    ASSERT_TRUE(result.success);

    // Count blocks in transformed code
    int transformed_blocks = 0;
    for (const auto& line : result.transformed_code) {
        // Count labels (block starts)
        if (line.find(':') != std::string::npos &&
            line.find('=') == std::string::npos &&
            line.find("define") == std::string::npos) {
            transformed_blocks++;
        }
    }

    // Flattening adds dispatcher, exit blocks, etc.
    EXPECT_GT(transformed_blocks, original_blocks)
        << "Flattened code should have more blocks";
}

// ============================================================================
// Dead Code Verification Tests
// ============================================================================

class DeadCodeVerificationTest : public ::testing::Test {
protected:
    cff::DeadCodeGenerator generator;
};

TEST_F(DeadCodeVerificationTest, GeneratesExpectedLineCount) {
    std::vector<std::string> vars = {"%a", "%b", "%c"};

    for (int count = 1; count <= 10; count++) {
        auto code = generator.generateLLVM(vars, count);
        EXPECT_EQ(static_cast<int>(code.size()), count)
            << "Should generate exactly " << count << " lines";
    }
}

TEST_F(DeadCodeVerificationTest, GeneratesValidInstructions) {
    std::vector<std::string> vars = {"%x", "%y"};
    auto code = generator.generateLLVM(vars, 20);

    for (const auto& line : code) {
        // Each line should be a valid LLVM instruction
        bool valid_instr =
            line.find("add") != std::string::npos ||
            line.find("sub") != std::string::npos ||
            line.find("mul") != std::string::npos ||
            line.find("xor") != std::string::npos ||
            line.find("and") != std::string::npos ||
            line.find("or") != std::string::npos ||
            line.find("shl") != std::string::npos ||
            line.find("lshr") != std::string::npos ||
            line.find("ashr") != std::string::npos;

        EXPECT_TRUE(valid_instr) << "Invalid instruction: " << line;
    }
}

TEST_F(DeadCodeVerificationTest, UsesProvidedVariables) {
    // Generator requires at least 2 variables to use them
    std::vector<std::string> vars = {"%unique_var_123", "%another_var_456"};
    auto code = generator.generateLLVM(vars, 10);

    bool uses_var = false;
    for (const auto& line : code) {
        if (line.find("%unique_var_123") != std::string::npos ||
            line.find("%another_var_456") != std::string::npos) {
            uses_var = true;
            break;
        }
    }

    // Generator should use provided variables
    EXPECT_TRUE(uses_var) << "Dead code should reference provided variables";
}

TEST_F(DeadCodeVerificationTest, CreatesUniqueVariables) {
    // Need at least 2 vars for the generator to use them
    std::vector<std::string> vars = {"%a", "%b"};
    auto code = generator.generateLLVM(vars, 10);

    // Count unique result variables created
    // The generator uses %_dead_ prefix (note underscore at end)
    std::set<std::string> created_vars;
    std::regex var_re("%_dead_[0-9]+");

    for (const auto& line : code) {
        std::sregex_iterator iter(line.begin(), line.end(), var_re);
        std::sregex_iterator end;
        for (; iter != end; ++iter) {
            created_vars.insert(iter->str());
        }
    }

    // Should create unique variables
    EXPECT_GE(created_vars.size(), 1u)
        << "Dead code should create new variables";
}

// ============================================================================
// Integration: Full Pipeline Verification
// ============================================================================

class FullPipelineVerificationTest : public LLVMIRFixture {
protected:
    void SetUp() override {
        LLVMIRFixture::SetUp();
    }
};

TEST_F(FullPipelineVerificationTest, MBAChangesCode) {
    // Test that MBA actually modifies the output
    const char* ir = R"(
define i32 @compute(i32 %a, i32 %b) {
entry:
  %sum = add i32 %a, %b
  %diff = sub i32 %a, %b
  %result = xor i32 %sum, %diff
  ret i32 %result
}
)";

    // Use --mba flag to enable MBA transformations with high probability
    // Note: when no config is given and extra_args is empty, --mba is added by default
    auto obfuscated = obfuscateIR(ir, "", "--mba --probability 1.0");

    if (!obfuscated.empty()) {
        // The obfuscated code should be different from original
        EXPECT_NE(std::string(ir), obfuscated) << "Obfuscated code should differ from original";

        // Should have MBA-style operations (xor, and, or, sub, shl for various MBA variants)
        int mba_patterns = 0;
        if (obfuscated.find("= xor ") != std::string::npos) mba_patterns++;
        if (obfuscated.find("= and ") != std::string::npos) mba_patterns++;
        if (obfuscated.find("= or ") != std::string::npos) mba_patterns++;
        if (obfuscated.find("= shl ") != std::string::npos) mba_patterns++;
        if (obfuscated.find("= sub ") != std::string::npos) mba_patterns++;

        // Original has xor, so we need at least one more pattern
        EXPECT_GE(mba_patterns, 2)
            << "Obfuscated code should have multiple MBA patterns";

        // Code should be longer
        EXPECT_GT(obfuscated.length(), strlen(ir))
            << "Obfuscated code should be longer";
    } else {
        // If morphect-ir isn't built or available, skip
        GTEST_SKIP() << "morphect-ir not available";
    }
}

// ============================================================================
// Quantitative Verification Tests
// ============================================================================

class QuantitativeVerificationTest : public ::testing::Test {
protected:
    mba::LLVMMBAPass mba_pass;
    mba::MBAPassConfig config;

    void SetUp() override {
        config.global.enabled = true;
        config.global.probability = 1.0;
        config.global.nesting_depth = 1;
        config.enable_add = true;
        config.enable_sub = true;
        config.enable_xor = true;
        config.enable_and = true;
        config.enable_or = true;
        mba_pass.initializeMBA(config);
    }
};

TEST_F(QuantitativeVerificationTest, MBAIncreasesInstructionCount) {
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %t1 = add i32 %a, %b",
        "  %t2 = xor i32 %t1, %a",
        "  %t3 = and i32 %t2, %b",
        "  %t4 = or i32 %t3, %a",
        "  %t5 = sub i32 %t4, %b",
        "  ret i32 %t5",
        "}"
    };

    size_t original_count = lines.size();

    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success);

    // Each MBA transformation replaces 1 line with 2-4 instructions
    // 5 operations * minimum 1 extra instruction = at least 5 more lines
    // Using conservative threshold since variant selection is random
    EXPECT_GT(lines.size(), original_count + 4)
        << "MBA should increase instruction count (have: " << lines.size()
        << ", original: " << original_count << ")";
}

TEST_F(QuantitativeVerificationTest, MBATransformationRate) {
    // Test that transformations happen at expected rate
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %t1 = add i32 %a, %b",
        "  %t2 = add i32 %t1, %a",
        "  %t3 = add i32 %t2, %b",
        "  ret i32 %t3",
        "}"
    };

    // Count original adds
    int original_adds = 0;
    for (const auto& line : lines) {
        if (line.find("= add i32") != std::string::npos) {
            original_adds++;
        }
    }
    ASSERT_EQ(original_adds, 3);

    auto result = mba_pass.transformIR(lines);
    EXPECT_EQ(result, TransformResult::Success);

    // With 100% probability, all adds should be transformed
    int remaining_simple_adds = 0;
    for (const auto& line : lines) {
        // Count adds that look like the original pattern (simple two-operand)
        if ((line.find("add i32 %a, %b") != std::string::npos) ||
            (line.find("add i32 %t1, %a") != std::string::npos) ||
            (line.find("add i32 %t2, %b") != std::string::npos)) {
            remaining_simple_adds++;
        }
    }

    EXPECT_EQ(remaining_simple_adds, 0)
        << "All original adds should be transformed at 100% probability";
}
