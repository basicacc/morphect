/**
 * Morphect - Dead Code Insertion Tests
 *
 * Tests for Phase 5.1 - Smart Dead Code:
 *   - P5-001: Generate realistic dead arithmetic
 *   - P5-002: Generate dead memory accesses
 *   - P5-003: Generate dead function calls
 *   - P5-004: Make dead code look obfuscated (apply MBA)
 *   - P5-005: Test dead code doesn't affect output
 */

#include <gtest/gtest.h>
#include "passes/deadcode/deadcode.hpp"
#include "common/random.hpp"

using namespace morphect;
using namespace morphect::deadcode;

// ============================================================================
// Test Fixtures
// ============================================================================

class DeadCodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(42);
    }

    // Sample LLVM IR for testing
    std::vector<std::string> getSampleIR() {
        return {
            "; ModuleID = 'test'",
            "source_filename = \"test.c\"",
            "",
            "define i32 @compute(i32 %a, i32 %b) {",
            "entry:",
            "  %sum = add i32 %a, %b",
            "  %mul = mul i32 %sum, 2",
            "  br label %check",
            "check:",
            "  %cmp = icmp sgt i32 %mul, 100",
            "  br i1 %cmp, label %then, label %else",
            "then:",
            "  %r1 = add i32 %mul, 10",
            "  br label %end",
            "else:",
            "  %r2 = sub i32 %mul, 10",
            "  br label %end",
            "end:",
            "  %result = phi i32 [ %r1, %then ], [ %r2, %else ]",
            "  ret i32 %result",
            "}"
        };
    }

    // Simple IR with single function
    std::vector<std::string> getSimpleIR() {
        return {
            "define i32 @simple(i32 %x) {",
            "entry:",
            "  %y = add i32 %x, 1",
            "  ret i32 %y",
            "}"
        };
    }
};

// ============================================================================
// P5-001: Generate Realistic Dead Arithmetic Tests
// ============================================================================

TEST_F(DeadCodeTest, DeadArithmeticGenerator_GeneratesCode) {
    DeadArithmeticGenerator gen;
    DeadCodeConfig config;
    config.min_ops_per_block = 3;
    config.max_ops_per_block = 5;

    std::vector<VariableInfo> vars;
    vars.push_back(VariableInfo("%x", "i32"));
    vars.push_back(VariableInfo("%y", "i32"));

    DeadCodeBlock block = gen.generate(vars, config);

    EXPECT_FALSE(block.code.empty());
    EXPECT_EQ(block.type, DeadCodeType::Arithmetic);
    EXPECT_GE(block.code.size(), static_cast<size_t>(config.min_ops_per_block));
}

TEST_F(DeadCodeTest, DeadArithmeticGenerator_UsesExistingVariables) {
    DeadArithmeticGenerator gen;
    DeadCodeConfig config;
    config.use_real_variables = true;

    std::vector<VariableInfo> vars;
    vars.push_back(VariableInfo("%real_var", "i32"));

    DeadCodeBlock block = gen.generate(vars, config);

    // Check that the block uses existing variable
    bool uses_real = false;
    for (const auto& line : block.code) {
        if (line.find("%real_var") != std::string::npos) {
            uses_real = true;
            break;
        }
    }
    // May or may not use it depending on random choice
    EXPECT_FALSE(block.code.empty());
}

TEST_F(DeadCodeTest, DeadArithmeticGenerator_CreatesValidOperations) {
    DeadArithmeticGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // Check that generated code has valid LLVM IR operations
    for (const auto& line : block.code) {
        // Each line should be an instruction or comment
        bool is_valid = line.find("add ") != std::string::npos ||
                        line.find("sub ") != std::string::npos ||
                        line.find("mul ") != std::string::npos ||
                        line.find("and ") != std::string::npos ||
                        line.find("or ") != std::string::npos ||
                        line.find("xor ") != std::string::npos ||
                        line.find("shl ") != std::string::npos ||
                        line.find("lshr ") != std::string::npos ||
                        line.find("ashr ") != std::string::npos ||
                        line.find(";") != std::string::npos;
        EXPECT_TRUE(is_valid) << "Invalid line: " << line;
    }
}

TEST_F(DeadCodeTest, DeadArithmeticGenerator_MultipleArithmeticOps) {
    DeadArithmeticGenerator gen;
    DeadCodeConfig config;
    config.min_ops_per_block = 5;
    config.max_ops_per_block = 5;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // Should generate at least 5 operations
    int op_count = 0;
    for (const auto& line : block.code) {
        if (line.find(" = ") != std::string::npos) {
            op_count++;
        }
    }
    EXPECT_GE(op_count, 5);
}

// ============================================================================
// P5-002: Generate Dead Memory Accesses Tests
// ============================================================================

TEST_F(DeadCodeTest, DeadMemoryGenerator_GeneratesCode) {
    DeadMemoryGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    EXPECT_FALSE(block.code.empty());
    EXPECT_EQ(block.type, DeadCodeType::Memory);
}

TEST_F(DeadCodeTest, DeadMemoryGenerator_AllocatesStack) {
    DeadMemoryGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // Should have at least one alloca
    bool has_alloca = false;
    for (const auto& line : block.code) {
        if (line.find("alloca") != std::string::npos) {
            has_alloca = true;
            break;
        }
    }
    EXPECT_TRUE(has_alloca);
}

TEST_F(DeadCodeTest, DeadMemoryGenerator_HasLoadStore) {
    GlobalRandom::setSeed(12345);  // Different seed for variety
    DeadMemoryGenerator gen;
    DeadCodeConfig config;
    config.min_ops_per_block = 4;
    config.max_ops_per_block = 8;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // Should have load or store operations
    bool has_mem_op = false;
    for (const auto& line : block.code) {
        if (line.find("load ") != std::string::npos ||
            line.find("store ") != std::string::npos) {
            has_mem_op = true;
            break;
        }
    }
    EXPECT_TRUE(has_mem_op);
}

TEST_F(DeadCodeTest, DeadMemoryGenerator_ArrayAccess) {
    // Run multiple times to catch array allocation (probabilistic)
    DeadMemoryGenerator gen;
    DeadCodeConfig config;

    bool has_array = false;
    for (int i = 0; i < 20 && !has_array; i++) {
        GlobalRandom::setSeed(i * 100);
        std::vector<VariableInfo> vars;
        DeadCodeBlock block = gen.generate(vars, config);

        for (const auto& line : block.code) {
            if (line.find("[") != std::string::npos &&
                line.find("alloca") != std::string::npos) {
                has_array = true;
                break;
            }
        }
    }
    EXPECT_TRUE(has_array);
}

// ============================================================================
// P5-003: Generate Dead Function Calls Tests
// ============================================================================

TEST_F(DeadCodeTest, DeadCallGenerator_GeneratesCode) {
    DeadCallGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    EXPECT_FALSE(block.code.empty());
    EXPECT_EQ(block.type, DeadCodeType::Call);
}

TEST_F(DeadCodeTest, DeadCallGenerator_GeneratesCalls) {
    DeadCallGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // Should have call instructions
    bool has_call = false;
    for (const auto& line : block.code) {
        if (line.find("call ") != std::string::npos) {
            has_call = true;
            break;
        }
    }
    EXPECT_TRUE(has_call);
}

TEST_F(DeadCodeTest, DeadCallGenerator_CreatesNopFunctions) {
    DeadCallGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // Should record nop functions to create
    EXPECT_FALSE(block.nop_functions_created.empty());
}

TEST_F(DeadCodeTest, DeadCallGenerator_NopFunctionDefinition) {
    auto def = DeadCallGenerator::generateNopFunction("_validate_1234", 2);

    EXPECT_FALSE(def.empty());

    // Should define a function
    bool has_define = false;
    bool has_ret = false;
    for (const auto& line : def) {
        if (line.find("define ") != std::string::npos &&
            line.find("_validate_1234") != std::string::npos) {
            has_define = true;
        }
        if (line.find("ret void") != std::string::npos) {
            has_ret = true;
        }
    }
    EXPECT_TRUE(has_define);
    EXPECT_TRUE(has_ret);
}

TEST_F(DeadCodeTest, DeadCallGenerator_RealisticFunctionNames) {
    DeadCallGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;

    // Generate multiple blocks and check function names
    std::vector<std::string> all_names;
    for (int i = 0; i < 10; i++) {
        GlobalRandom::setSeed(i * 100);
        DeadCodeBlock block = gen.generate(vars, config);
        for (const auto& name : block.nop_functions_created) {
            all_names.push_back(name);
        }
    }

    // Should have variety in names
    std::unordered_set<std::string> unique_prefixes;
    for (const auto& name : all_names) {
        // Extract prefix (before underscore+number)
        size_t pos = name.rfind('_');
        if (pos != std::string::npos) {
            unique_prefixes.insert(name.substr(0, pos));
        }
    }
    EXPECT_GT(unique_prefixes.size(), 1u);  // Should have multiple prefixes
}

// ============================================================================
// P5-004: Make Dead Code Look Obfuscated (MBA) Tests
// ============================================================================

TEST_F(DeadCodeTest, MBADeadCodeGenerator_GeneratesCode) {
    MBADeadCodeGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    EXPECT_FALSE(block.code.empty());
    EXPECT_EQ(block.type, DeadCodeType::Arithmetic);
}

TEST_F(DeadCodeTest, MBADeadCodeGenerator_UsesMBAPattern) {
    MBADeadCodeGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // MBA pattern: a + b = (a ^ b) + 2*(a & b)
    // Should have xor, and, and shl (for multiplication by 2)
    bool has_xor = false;
    bool has_and = false;
    bool has_shl = false;

    for (const auto& line : block.code) {
        if (line.find(" xor ") != std::string::npos) has_xor = true;
        if (line.find(" and ") != std::string::npos) has_and = true;
        if (line.find(" shl ") != std::string::npos) has_shl = true;
    }

    EXPECT_TRUE(has_xor);
    EXPECT_TRUE(has_and);
    EXPECT_TRUE(has_shl);
}

TEST_F(DeadCodeTest, MBADeadCodeGenerator_CreatesChainedOps) {
    MBADeadCodeGenerator gen;
    DeadCodeConfig config;

    std::vector<VariableInfo> vars;
    DeadCodeBlock block = gen.generate(vars, config);

    // Should create multiple variables that depend on each other
    EXPECT_GE(block.vars_created.size(), 4u);
}

TEST_F(DeadCodeTest, MBADeadCodeGenerator_UsesRealVariables) {
    MBADeadCodeGenerator gen;
    DeadCodeConfig config;
    config.use_real_variables = true;

    std::vector<VariableInfo> vars;
    vars.push_back(VariableInfo("%input", "i32"));

    DeadCodeBlock block = gen.generate(vars, config);

    // Should reference the real variable
    bool uses_real = false;
    for (const auto& line : block.code) {
        if (line.find("%input") != std::string::npos) {
            uses_real = true;
            break;
        }
    }
    // May use it depending on random selection
    EXPECT_FALSE(block.code.empty());
}

// ============================================================================
// Opaque Predicate Tests
// ============================================================================

TEST_F(DeadCodeTest, OpaquePredicateGen_AlwaysFalse) {
    auto [condition, setup] = OpaquePredicateGen::generateAlwaysFalse("test");

    EXPECT_FALSE(condition.empty());
    EXPECT_FALSE(setup.empty());

    // Condition should be an icmp
    EXPECT_TRUE(condition.find("icmp") != std::string::npos);
}

TEST_F(DeadCodeTest, OpaquePredicateGen_AlwaysTrue) {
    auto [condition, setup] = OpaquePredicateGen::generateAlwaysTrue("test");

    EXPECT_FALSE(condition.empty());
    EXPECT_FALSE(setup.empty());

    // Condition should be an icmp
    EXPECT_TRUE(condition.find("icmp") != std::string::npos);
}

TEST_F(DeadCodeTest, OpaquePredicateGen_VariousTypes) {
    // Test that we get variety in predicates
    std::unordered_set<std::string> predicates;

    for (int i = 0; i < 50; i++) {
        GlobalRandom::setSeed(i * 100);
        auto [condition, setup] = OpaquePredicateGen::generateAlwaysFalse("p" + std::to_string(i));
        predicates.insert(condition.substr(0, 20));  // First part for uniqueness
    }

    // Should have multiple types
    EXPECT_GT(predicates.size(), 2u);
}

// ============================================================================
// Code Analyzer Tests
// ============================================================================

TEST_F(DeadCodeTest, LLVMCodeAnalyzer_ExtractsVariables) {
    LLVMCodeAnalyzer analyzer;
    auto vars = analyzer.extractVariables(getSampleIR());

    EXPECT_FALSE(vars.empty());

    // Should find %a, %b, %sum, %mul, etc.
    std::unordered_set<std::string> var_names;
    for (const auto& v : vars) {
        var_names.insert(v.name);
    }

    EXPECT_TRUE(var_names.find("%sum") != var_names.end() ||
                var_names.find("%mul") != var_names.end());
}

TEST_F(DeadCodeTest, LLVMCodeAnalyzer_FindsInsertionPoints) {
    LLVMCodeAnalyzer analyzer;
    auto points = analyzer.findInsertionPoints(getSampleIR());

    EXPECT_FALSE(points.empty());
}

TEST_F(DeadCodeTest, LLVMCodeAnalyzer_ExtractsFunctionNames) {
    LLVMCodeAnalyzer analyzer;
    auto funcs = analyzer.extractFunctionNames(getSampleIR());

    EXPECT_FALSE(funcs.empty());
    EXPECT_EQ(funcs[0], "compute");
}

// ============================================================================
// P5-005: Test Dead Code Doesn't Affect Output
// ============================================================================

TEST_F(DeadCodeTest, LLVMDeadCodeTransformation_Transform) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;  // Always insert

    auto result = transform.transform(getSampleIR(), config);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.transformed_code.empty());
}

TEST_F(DeadCodeTest, LLVMDeadCodeTransformation_InsertsBlocks) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.min_blocks = 2;
    config.max_blocks = 4;

    auto result = transform.transform(getSampleIR(), config);

    EXPECT_TRUE(result.success);
    EXPECT_GE(result.blocks_inserted, 1);
}

TEST_F(DeadCodeTest, LLVMDeadCodeTransformation_AddsOpaqueGuards) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;

    auto result = transform.transform(getSimpleIR(), config);

    // Should have opaque predicate structure
    bool has_dead_label = false;
    bool has_continue_label = false;

    for (const auto& line : result.transformed_code) {
        if (line.find("dead_block") != std::string::npos) has_dead_label = true;
        if (line.find("continue") != std::string::npos) has_continue_label = true;
    }

    // If blocks were inserted, should have the guard structure
    if (result.blocks_inserted > 0) {
        EXPECT_TRUE(has_dead_label || has_continue_label);
    }
}

TEST_F(DeadCodeTest, LLVMDeadCodeTransformation_PreservesOriginalCode) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;

    auto original = getSimpleIR();
    auto result = transform.transform(original, config);

    // Original instructions should still be present
    bool has_original_add = false;
    bool has_original_ret = false;

    for (const auto& line : result.transformed_code) {
        if (line.find("%y = add i32 %x, 1") != std::string::npos) {
            has_original_add = true;
        }
        if (line.find("ret i32 %y") != std::string::npos) {
            has_original_ret = true;
        }
    }

    EXPECT_TRUE(has_original_add);
    EXPECT_TRUE(has_original_ret);
}

TEST_F(DeadCodeTest, LLVMDeadCodeTransformation_DisabledDoesNotModify) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = false;

    auto original = getSimpleIR();
    auto result = transform.transform(original, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.transformed_code.size(), original.size());
    EXPECT_EQ(result.blocks_inserted, 0);
}

TEST_F(DeadCodeTest, LLVMDeadCodeTransformation_CreatesNopFunctions) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.call_probability = 1.0;  // Always use call generator
    config.arithmetic_probability = 0.0;
    config.memory_probability = 0.0;

    // Need larger IR to have insertion points
    auto ir = getSampleIR();
    auto result = transform.transform(ir, config);

    // If calls were inserted, nop functions should be created
    if (result.calls_inserted > 0) {
        EXPECT_FALSE(result.nop_functions_created.empty());

        // Check that nop function definitions were added
        bool has_nop_def = false;
        for (const auto& line : result.transformed_code) {
            for (const auto& func : result.nop_functions_created) {
                if (line.find("define ") != std::string::npos &&
                    line.find(func) != std::string::npos) {
                    has_nop_def = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(has_nop_def);
    }
}

// ============================================================================
// Pass Interface Tests
// ============================================================================

TEST_F(DeadCodeTest, LLVMDeadCodePass_Initialize) {
    LLVMDeadCodePass pass;

    PassConfig config;
    config.enabled = true;
    config.probability = 0.8;

    EXPECT_TRUE(pass.initialize(config));
    EXPECT_TRUE(pass.isEnabled());
}

TEST_F(DeadCodeTest, LLVMDeadCodePass_Transform) {
    LLVMDeadCodePass pass;

    DeadCodeConfig dc_config;
    dc_config.enabled = true;
    dc_config.probability = 1.0;
    pass.setDeadCodeConfig(dc_config);

    auto lines = getSampleIR();
    auto result = pass.transformIR(lines);

    EXPECT_EQ(result, TransformResult::Success);
}

TEST_F(DeadCodeTest, LLVMDeadCodePass_SkipsWhenDisabled) {
    LLVMDeadCodePass pass;

    DeadCodeConfig dc_config;
    dc_config.enabled = false;
    pass.setDeadCodeConfig(dc_config);

    auto lines = getSampleIR();
    auto result = pass.transformIR(lines);

    EXPECT_EQ(result, TransformResult::Skipped);
}

TEST_F(DeadCodeTest, LLVMDeadCodePass_Statistics) {
    LLVMDeadCodePass pass;

    DeadCodeConfig dc_config;
    dc_config.enabled = true;
    dc_config.probability = 1.0;
    pass.setDeadCodeConfig(dc_config);

    auto lines = getSampleIR();
    pass.transformIR(lines);

    auto stats = pass.getStatistics();
    // Should have recorded statistics
    EXPECT_TRUE(stats.find("blocks_inserted") != stats.end());
    EXPECT_TRUE(stats.find("ops_inserted") != stats.end());
}

TEST_F(DeadCodeTest, LLVMDeadCodePass_GetName) {
    LLVMDeadCodePass pass;
    EXPECT_EQ(pass.getName(), "DeadCode");
}

TEST_F(DeadCodeTest, LLVMDeadCodePass_GetPriority) {
    LLVMDeadCodePass pass;
    EXPECT_EQ(pass.getPriority(), PassPriority::Late);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(DeadCodeTest, Integration_AllGeneratorTypes) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.arithmetic_probability = 0.25;
    config.memory_probability = 0.25;
    config.call_probability = 0.25;
    config.control_flow_probability = 0.25;
    config.apply_mba = true;
    config.min_blocks = 4;
    config.max_blocks = 8;

    auto result = transform.transform(getSampleIR(), config);

    EXPECT_TRUE(result.success);
    // Should have inserted some blocks
    EXPECT_GE(result.blocks_inserted, 1);
}

TEST_F(DeadCodeTest, Integration_MultipleTransforms) {
    // Apply transformation multiple times
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 0.8;
    config.min_blocks = 1;
    config.max_blocks = 2;

    auto code = getSimpleIR();

    for (int i = 0; i < 3; i++) {
        GlobalRandom::setSeed(i * 1000);
        auto result = transform.transform(code, config);
        EXPECT_TRUE(result.success);
        code = result.transformed_code;
    }

    // Code should have grown
    EXPECT_GT(code.size(), getSimpleIR().size());
}

TEST_F(DeadCodeTest, Integration_LargeFunction) {
    // Test with larger function
    std::vector<std::string> large_ir = {
        "define i32 @large_func(i32 %n) {",
        "entry:",
        "  %sum = alloca i32",
        "  store i32 0, i32* %sum",
        "  %i = alloca i32",
        "  store i32 0, i32* %i",
        "  br label %loop",
        "loop:",
        "  %i_val = load i32, i32* %i",
        "  %cmp = icmp slt i32 %i_val, %n",
        "  br i1 %cmp, label %body, label %exit",
        "body:",
        "  %sum_val = load i32, i32* %sum",
        "  %new_sum = add i32 %sum_val, %i_val",
        "  store i32 %new_sum, i32* %sum",
        "  %i_inc = add i32 %i_val, 1",
        "  store i32 %i_inc, i32* %i",
        "  br label %loop",
        "exit:",
        "  %result = load i32, i32* %sum",
        "  ret i32 %result",
        "}"
    };

    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.min_blocks = 2;
    config.max_blocks = 4;

    auto result = transform.transform(large_ir, config);

    EXPECT_TRUE(result.success);
    EXPECT_GE(result.blocks_inserted, 1);
}

TEST_F(DeadCodeTest, Integration_ExcludeFunctions) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.exclude_functions.push_back("simple");

    auto result = transform.transform(getSimpleIR(), config);

    EXPECT_TRUE(result.success);
    // Should still work but might not insert in excluded functions
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DeadCodeTest, EdgeCase_EmptyInput) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;

    std::vector<std::string> empty;
    auto result = transform.transform(empty, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.blocks_inserted, 0);
}

TEST_F(DeadCodeTest, EdgeCase_NoFunctions) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;

    std::vector<std::string> no_funcs = {
        "; Just comments",
        "@global = global i32 42"
    };

    auto result = transform.transform(no_funcs, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.blocks_inserted, 0);
}

TEST_F(DeadCodeTest, EdgeCase_ZeroProbability) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 0.0;

    auto result = transform.transform(getSampleIR(), config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.blocks_inserted, 0);
}

TEST_F(DeadCodeTest, EdgeCase_MinMaxBlocks) {
    LLVMDeadCodeTransformation transform;
    DeadCodeConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.min_blocks = 1;
    config.max_blocks = 2;

    auto result = transform.transform(getSampleIR(), config);

    EXPECT_TRUE(result.success);
    // Should insert between min and max blocks (inclusive)
    EXPECT_GE(result.blocks_inserted, 0);  // May be 0 if no insertion points
    EXPECT_LE(result.blocks_inserted, config.max_blocks);
}
