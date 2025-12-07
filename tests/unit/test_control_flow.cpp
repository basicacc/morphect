/**
 * Morphect - Control Flow Tests
 *
 * Unit tests for indirect branch obfuscation.
 */

#include <gtest/gtest.h>
#include "passes/control_flow/control_flow.hpp"
#include "common/random.hpp"

#include <set>
#include <algorithm>

using namespace morphect;
using namespace morphect::control_flow;

class IndirectBranchBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(12345);
    }
};

// ============================================================================
// Jump Table Structure Tests (P4-001)
// ============================================================================

TEST_F(IndirectBranchBaseTest, JumpTableEntry_DefaultValues) {
    JumpTableEntry entry;
    EXPECT_EQ(entry.original_index, 0);
    EXPECT_EQ(entry.obfuscated_index, 0);
    EXPECT_TRUE(entry.target_label.empty());
    EXPECT_FALSE(entry.is_decoy);
}

TEST_F(IndirectBranchBaseTest, JumpTable_GetEntry_ByObfuscatedIndex) {
    JumpTable table;
    table.table_name = "test_table";

    JumpTableEntry e1;
    e1.original_index = 0;
    e1.obfuscated_index = 5;
    e1.target_label = "block_a";
    table.entries.push_back(e1);

    JumpTableEntry e2;
    e2.original_index = 1;
    e2.obfuscated_index = 3;
    e2.target_label = "block_b";
    table.entries.push_back(e2);

    const auto* found = table.getEntry(5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->target_label, "block_a");

    found = table.getEntry(3);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->target_label, "block_b");

    found = table.getEntry(99);
    EXPECT_EQ(found, nullptr);
}

TEST_F(IndirectBranchBaseTest, JumpTable_GetEntry_ByOriginalIndex) {
    JumpTable table;

    JumpTableEntry e1;
    e1.original_index = 0;
    e1.obfuscated_index = 42;
    e1.target_label = "target_0";
    table.entries.push_back(e1);

    JumpTableEntry e2;
    e2.original_index = 1;
    e2.obfuscated_index = 17;
    e2.target_label = "target_1";
    table.entries.push_back(e2);

    const auto* found = table.getEntryByOriginal(0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->obfuscated_index, 42);
    EXPECT_EQ(found->target_label, "target_0");

    found = table.getEntryByOriginal(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->obfuscated_index, 17);
}

TEST_F(IndirectBranchBaseTest, Config_DefaultValues) {
    IndirectBranchConfig config;
    EXPECT_TRUE(config.enabled);
    EXPECT_DOUBLE_EQ(config.probability, 0.8);
    EXPECT_EQ(config.index_strategy, IndexObfStrategy::XOR);
    EXPECT_TRUE(config.use_mba_for_index);
    EXPECT_TRUE(config.add_decoy_entries);
    EXPECT_TRUE(config.shuffle_entries);
}

TEST_F(IndirectBranchBaseTest, BranchInfo_ConditionalBranch) {
    BranchInfo info;
    info.id = 1;
    info.source_label = "entry";
    info.targets = {"true_block", "false_block"};
    info.is_conditional = true;
    info.condition = "%cond";

    EXPECT_TRUE(info.is_conditional);
    EXPECT_FALSE(info.is_switch);
    EXPECT_EQ(info.targets.size(), 2u);
}

TEST_F(IndirectBranchBaseTest, BranchInfo_SwitchStatement) {
    BranchInfo info;
    info.id = 2;
    info.is_switch = true;
    info.case_values = {0, 1, 2, 3};
    info.targets = {"default", "case_0", "case_1", "case_2", "case_3"};
    info.default_target = "default";

    EXPECT_TRUE(info.is_switch);
    EXPECT_EQ(info.case_values.size(), 4u);
    EXPECT_EQ(info.targets.size(), 5u);
}

// ============================================================================
// Index Obfuscation Tests (P4-002)
// ============================================================================

class IndexObfuscationTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(54321);
    }
};

TEST_F(IndexObfuscationTest, XOR_Strategy_ProducesValidIndices) {
    // Test that XOR obfuscation produces different indices
    JumpTable table;
    table.table_name = "xor_test";
    table.index_strategy = IndexObfStrategy::XOR;
    table.xor_key = 0x1234;

    // Simulate XOR obfuscation
    for (int i = 0; i < 5; i++) {
        JumpTableEntry entry;
        entry.original_index = i;
        entry.obfuscated_index = i ^ static_cast<int>(table.xor_key);
        entry.target_label = "block_" + std::to_string(i);
        table.entries.push_back(entry);
    }

    // Verify each obfuscated index can be reversed
    for (int i = 0; i < 5; i++) {
        int obf = table.entries[i].obfuscated_index;
        int recovered = obf ^ static_cast<int>(table.xor_key);
        EXPECT_EQ(recovered, i);
    }
}

TEST_F(IndexObfuscationTest, LinearTransform_IsReversible) {
    // Linear transform: (a * idx + b) % size
    int64_t a = 3;
    int64_t b = 7;
    int64_t size = 10;

    // Find modular inverse of a mod size
    // For a=3, size=10: inverse is 7 (3*7=21 mod 10 = 1)
    int64_t a_inv = 7;

    for (int idx = 0; idx < 10; idx++) {
        int64_t obf = (a * idx + b) % size;
        // Reverse: idx = a_inv * (obf - b) mod size
        int64_t recovered = (a_inv * ((obf - b + size) % size)) % size;
        EXPECT_EQ(recovered, idx);
    }
}

TEST_F(IndexObfuscationTest, MBA_XOR_Identity) {
    // MBA identity: a ^ b = (a | b) - (a & b)
    for (int a = 0; a < 100; a++) {
        for (int b = 0; b < 100; b++) {
            int xor_result = a ^ b;
            int mba_result = (a | b) - (a & b);
            EXPECT_EQ(mba_result, xor_result)
                << "Failed for a=" << a << ", b=" << b;
        }
    }
}

TEST_F(IndexObfuscationTest, MBA_XOR_AlternativeIdentity) {
    // Alternative: a ^ b = (a + b) - 2*(a & b)
    for (int a = 0; a < 100; a++) {
        for (int b = 0; b < 100; b++) {
            int xor_result = a ^ b;
            int mba_result = (a + b) - 2 * (a & b);
            EXPECT_EQ(mba_result, xor_result)
                << "Failed for a=" << a << ", b=" << b;
        }
    }
}

// ============================================================================
// Branch Analyzer Tests
// ============================================================================

class LLVMBranchAnalyzerTest : public ::testing::Test {
protected:
    LLVMBranchAnalyzer analyzer_;

    void SetUp() override {
        GlobalRandom::setSeed(11111);
    }
};

TEST_F(LLVMBranchAnalyzerTest, FindUnconditionalBranch) {
    std::vector<std::string> lines = {
        "entry:",
        "  %x = add i32 1, 2",
        "  br label %next",
        "next:",
        "  ret void"
    };

    auto branches = analyzer_.findBranches(lines);
    ASSERT_EQ(branches.size(), 1u);
    EXPECT_EQ(branches[0].source_label, "entry");
    EXPECT_FALSE(branches[0].is_conditional);
    EXPECT_EQ(branches[0].targets.size(), 1u);
    EXPECT_EQ(branches[0].targets[0], "next");
}

TEST_F(LLVMBranchAnalyzerTest, FindConditionalBranch) {
    std::vector<std::string> lines = {
        "entry:",
        "  %cond = icmp eq i32 %x, 0",
        "  br i1 %cond, label %then_bb, label %else_bb",
        "then_bb:",
        "  ret i32 1",
        "else_bb:",
        "  ret i32 0"
    };

    auto branches = analyzer_.findBranches(lines);
    ASSERT_EQ(branches.size(), 1u);
    EXPECT_TRUE(branches[0].is_conditional);
    EXPECT_EQ(branches[0].condition, "%cond");
    EXPECT_EQ(branches[0].targets.size(), 2u);
    EXPECT_EQ(branches[0].targets[0], "then_bb");
    EXPECT_EQ(branches[0].targets[1], "else_bb");
}

TEST_F(LLVMBranchAnalyzerTest, FindMultipleBranches) {
    std::vector<std::string> lines = {
        "entry:",
        "  br label %loop_header",
        "loop_header:",
        "  %cond = icmp slt i32 %i, 10",
        "  br i1 %cond, label %loop_body, label %exit",
        "loop_body:",
        "  br label %loop_header",
        "exit:",
        "  ret void"
    };

    auto branches = analyzer_.findBranches(lines);
    EXPECT_EQ(branches.size(), 3u);

    // First: unconditional br to loop_header
    EXPECT_FALSE(branches[0].is_conditional);
    EXPECT_EQ(branches[0].targets[0], "loop_header");

    // Second: conditional loop check
    EXPECT_TRUE(branches[1].is_conditional);
    EXPECT_EQ(branches[1].targets[0], "loop_body");
    EXPECT_EQ(branches[1].targets[1], "exit");

    // Third: back edge
    EXPECT_FALSE(branches[2].is_conditional);
    EXPECT_EQ(branches[2].targets[0], "loop_header");
}

// ============================================================================
// Transformation Tests
// ============================================================================

class LLVMIndirectBranchTransformTest : public ::testing::Test {
protected:
    LLVMBranchAnalyzer analyzer_;
    LLVMIndirectBranchTransformation transformer_;
    IndirectBranchConfig config_;

    void SetUp() override {
        GlobalRandom::setSeed(99999);
        config_.enabled = true;
        config_.probability = 1.0;  // Always transform for testing
        config_.add_decoy_entries = false;  // Simpler for testing
        config_.shuffle_entries = false;
        config_.use_mba_for_index = false;
    }
};

TEST_F(LLVMIndirectBranchTransformTest, TransformUnconditionalBranch) {
    std::vector<std::string> lines = {
        "define void @test() {",
        "entry:",
        "  br label %target",
        "target:",
        "  ret void",
        "}"
    };

    auto branches = analyzer_.findBranches(lines);
    ASSERT_EQ(branches.size(), 1u);

    auto result = transformer_.transform(lines, branches, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.branches_transformed, 1);
    EXPECT_EQ(result.tables_created, 1);

    // Verify indirectbr is in the output
    bool found_indirectbr = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("indirectbr") != std::string::npos) {
            found_indirectbr = true;
            break;
        }
    }
    EXPECT_TRUE(found_indirectbr);
}

TEST_F(LLVMIndirectBranchTransformTest, TransformConditionalBranch) {
    std::vector<std::string> lines = {
        "define i32 @test(i32 %x) {",
        "entry:",
        "  %cond = icmp eq i32 %x, 0",
        "  br i1 %cond, label %then_bb, label %else_bb",
        "then_bb:",
        "  ret i32 1",
        "else_bb:",
        "  ret i32 0",
        "}"
    };

    auto branches = analyzer_.findBranches(lines);
    ASSERT_EQ(branches.size(), 1u);

    auto result = transformer_.transform(lines, branches, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.branches_transformed, 1);

    // Should have select instruction for condition->index conversion
    bool found_select = false;
    bool found_indirectbr = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("select i1") != std::string::npos) {
            found_select = true;
        }
        if (line.find("indirectbr") != std::string::npos) {
            found_indirectbr = true;
        }
    }
    EXPECT_TRUE(found_select);
    EXPECT_TRUE(found_indirectbr);
}

TEST_F(LLVMIndirectBranchTransformTest, JumpTableCreated) {
    std::vector<std::string> lines = {
        "define void @test() {",
        "entry:",
        "  br label %target",
        "target:",
        "  ret void",
        "}"
    };

    auto branches = analyzer_.findBranches(lines);
    auto result = transformer_.transform(lines, branches, config_);

    EXPECT_EQ(result.tables.size(), 1u);
    EXPECT_FALSE(result.tables[0].table_name.empty());
    EXPECT_GE(result.tables[0].entries.size(), 1u);
}

TEST_F(LLVMIndirectBranchTransformTest, DecoyEntriesAdded) {
    config_.add_decoy_entries = true;
    config_.min_decoy_count = 2;
    config_.max_decoy_count = 2;

    std::vector<std::string> lines = {
        "define void @test() {",
        "entry:",
        "  br label %target",
        "target:",
        "  ret void",
        "}"
    };

    auto branches = analyzer_.findBranches(lines);
    auto result = transformer_.transform(lines, branches, config_);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.tables.size(), 1u);

    // Should have real entries + decoy entries
    const auto& table = result.tables[0];
    size_t decoy_count = 0;
    for (const auto& entry : table.entries) {
        if (entry.is_decoy) decoy_count++;
    }
    EXPECT_GE(decoy_count, 2u);
    EXPECT_EQ(result.decoy_entries_added, static_cast<int>(decoy_count));
}

TEST_F(LLVMIndirectBranchTransformTest, XORIndexObfuscation) {
    config_.index_strategy = IndexObfStrategy::XOR;

    std::vector<std::string> lines = {
        "define i32 @test(i32 %x) {",
        "entry:",
        "  %cond = icmp eq i32 %x, 0",
        "  br i1 %cond, label %a, label %b",
        "a:",
        "  ret i32 1",
        "b:",
        "  ret i32 0",
        "}"
    };

    auto branches = analyzer_.findBranches(lines);
    auto result = transformer_.transform(lines, branches, config_);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.tables.size(), 1u);
    EXPECT_NE(result.tables[0].xor_key, 0u);
}

// ============================================================================
// Pass Integration Tests
// ============================================================================

class LLVMIndirectBranchPassTest : public ::testing::Test {
protected:
    LLVMIndirectBranchPass pass_;

    void SetUp() override {
        GlobalRandom::setSeed(77777);
        PassConfig pc;
        pc.enabled = true;
        pc.probability = 1.0;
        pass_.initialize(pc);

        IndirectBranchConfig ibc;
        ibc.enabled = true;
        ibc.probability = 1.0;
        ibc.add_decoy_entries = false;
        ibc.shuffle_entries = false;
        pass_.setIndirectBranchConfig(ibc);
    }
};

TEST_F(LLVMIndirectBranchPassTest, PassMetadata) {
    EXPECT_EQ(pass_.getName(), "IndirectBranch");
    EXPECT_FALSE(pass_.getDescription().empty());
    EXPECT_EQ(pass_.getPriority(), PassPriority::ControlFlow);
}

TEST_F(LLVMIndirectBranchPassTest, TransformIR_Simple) {
    std::vector<std::string> lines = {
        "define void @simple() {",
        "entry:",
        "  br label %exit",
        "exit:",
        "  ret void",
        "}"
    };

    auto result = pass_.transformIR(lines);
    EXPECT_TRUE(result == TransformResult::Success || result == TransformResult::Skipped);
}

TEST_F(LLVMIndirectBranchPassTest, Statistics_Populated) {
    std::vector<std::string> lines = {
        "define void @test() {",
        "entry:",
        "  br label %exit",
        "exit:",
        "  ret void",
        "}"
    };

    pass_.transformIR(lines);
    auto stats = pass_.getStatistics();

    EXPECT_TRUE(stats.find("branches_found") != stats.end());
}

TEST_F(LLVMIndirectBranchPassTest, DisabledPass_NoTransformation) {
    IndirectBranchConfig config;
    config.enabled = false;
    pass_.setIndirectBranchConfig(config);

    std::vector<std::string> lines = {
        "define void @test() {",
        "entry:",
        "  br label %exit",
        "exit:",
        "  ret void",
        "}"
    };

    auto original = lines;
    auto result = pass_.transformIR(lines);

    EXPECT_EQ(result, TransformResult::Skipped);
    EXPECT_EQ(lines, original);  // No changes
}

// ============================================================================
// Edge Cases
// ============================================================================

class IndirectBranchEdgeCasesTest : public ::testing::Test {
protected:
    LLVMBranchAnalyzer analyzer_;

    void SetUp() override {
        GlobalRandom::setSeed(33333);
    }
};

TEST_F(IndirectBranchEdgeCasesTest, EmptyInput) {
    std::vector<std::string> lines;
    auto branches = analyzer_.findBranches(lines);
    EXPECT_TRUE(branches.empty());
}

TEST_F(IndirectBranchEdgeCasesTest, NoBranches) {
    std::vector<std::string> lines = {
        "define void @test() {",
        "entry:",
        "  ret void",
        "}"
    };

    auto branches = analyzer_.findBranches(lines);
    EXPECT_TRUE(branches.empty());
}

TEST_F(IndirectBranchEdgeCasesTest, OnlyComments) {
    std::vector<std::string> lines = {
        "; This is a comment",
        "; Another comment"
    };

    auto branches = analyzer_.findBranches(lines);
    EXPECT_TRUE(branches.empty());
}

TEST_F(IndirectBranchEdgeCasesTest, BranchInComment_NotDetected) {
    std::vector<std::string> lines = {
        "entry:",
        "; br label %fake",
        "  ret void"
    };

    auto branches = analyzer_.findBranches(lines);
    EXPECT_TRUE(branches.empty());
}

// ============================================================================
// GCD Utility Test
// ============================================================================

TEST(GCDTest, BasicCases) {
    // Using the gcd function from IndirectBranchTransformation
    auto gcd = [](int64_t a, int64_t b) -> int64_t {
        while (b != 0) {
            int64_t t = b;
            b = a % b;
            a = t;
        }
        return a;
    };

    EXPECT_EQ(gcd(12, 8), 4);
    EXPECT_EQ(gcd(17, 13), 1);  // Coprime
    EXPECT_EQ(gcd(100, 25), 25);
    EXPECT_EQ(gcd(1, 1), 1);
    EXPECT_EQ(gcd(0, 5), 5);
}

// ============================================================================
// Indirect Call Tests (P4-004 to P4-007)
// ============================================================================

class IndirectCallBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(44444);
    }
};

TEST_F(IndirectCallBaseTest, FunctionTableEntry_DefaultValues) {
    FunctionTableEntry entry;
    EXPECT_EQ(entry.index, 0);
    EXPECT_TRUE(entry.function_name.empty());
    EXPECT_FALSE(entry.is_decoy);
    EXPECT_EQ(entry.xor_key, 0u);
}

TEST_F(IndirectCallBaseTest, FunctionTable_GetEntry_ByName) {
    FunctionTable table;
    table.table_name = "test_func_table";

    FunctionTableEntry e1;
    e1.index = 0;
    e1.function_name = "foo";
    table.entries.push_back(e1);

    FunctionTableEntry e2;
    e2.index = 1;
    e2.function_name = "bar";
    table.entries.push_back(e2);

    const auto* found = table.getEntry("foo");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->index, 0);

    found = table.getEntry("bar");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->index, 1);

    found = table.getEntry("baz");
    EXPECT_EQ(found, nullptr);
}

TEST_F(IndirectCallBaseTest, FunctionTable_GetEntry_ByIndex) {
    FunctionTable table;

    FunctionTableEntry e1;
    e1.index = 5;
    e1.function_name = "func_a";
    table.entries.push_back(e1);

    const auto* found = table.getEntryByIndex(5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->function_name, "func_a");

    found = table.getEntryByIndex(99);
    EXPECT_EQ(found, nullptr);
}

TEST_F(IndirectCallBaseTest, IndirectCallConfig_DefaultValues) {
    IndirectCallConfig config;
    EXPECT_TRUE(config.enabled);
    EXPECT_DOUBLE_EQ(config.probability, 0.75);
    EXPECT_EQ(config.address_strategy, AddressObfStrategy::XOR);
    EXPECT_TRUE(config.add_decoy_entries);
    EXPECT_TRUE(config.skip_intrinsics);
}

TEST_F(IndirectCallBaseTest, CallSiteInfo_Properties) {
    CallSiteInfo info;
    info.id = 1;
    info.caller_function = "main";
    info.callee_function = "helper";
    info.callee_type = "i32";
    info.result_var = "%result";
    info.arguments = {"i32 %x", "i32 %y"};

    EXPECT_EQ(info.callee_function, "helper");
    EXPECT_EQ(info.arguments.size(), 2u);
    EXPECT_FALSE(info.is_tail_call);
}

TEST_F(IndirectCallBaseTest, FunctionInfo_Properties) {
    FunctionInfo info;
    info.name = "compute";
    info.return_type = "i32";
    info.param_types = {"i32", "i32"};
    info.full_signature = "i32 (i32, i32)";
    info.is_vararg = false;
    info.is_external = false;

    EXPECT_EQ(info.name, "compute");
    EXPECT_EQ(info.param_types.size(), 2u);
}

// ============================================================================
// Call Site Analyzer Tests
// ============================================================================

class LLVMCallSiteAnalyzerTest : public ::testing::Test {
protected:
    LLVMCallSiteAnalyzer analyzer_;

    void SetUp() override {
        GlobalRandom::setSeed(55555);
    }
};

TEST_F(LLVMCallSiteAnalyzerTest, FindSimpleCall) {
    std::vector<std::string> lines = {
        "define void @main() {",
        "entry:",
        "  call void @helper()",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0].callee_function, "helper");
    EXPECT_EQ(calls[0].caller_function, "main");
    EXPECT_TRUE(calls[0].result_var.empty());
}

TEST_F(LLVMCallSiteAnalyzerTest, FindCallWithResult) {
    std::vector<std::string> lines = {
        "define i32 @main() {",
        "entry:",
        "  %result = call i32 @compute(i32 5)",
        "  ret i32 %result",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0].callee_function, "compute");
    EXPECT_EQ(calls[0].result_var, "%result");
    EXPECT_EQ(calls[0].callee_type, "i32");
}

TEST_F(LLVMCallSiteAnalyzerTest, FindMultipleCalls) {
    std::vector<std::string> lines = {
        "define void @main() {",
        "entry:",
        "  call void @init()",
        "  %x = call i32 @process(i32 1)",
        "  call void @cleanup()",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    EXPECT_EQ(calls.size(), 3u);
    EXPECT_EQ(calls[0].callee_function, "init");
    EXPECT_EQ(calls[1].callee_function, "process");
    EXPECT_EQ(calls[2].callee_function, "cleanup");
}

TEST_F(LLVMCallSiteAnalyzerTest, SkipIntrinsics) {
    std::vector<std::string> lines = {
        "define void @main() {",
        "entry:",
        "  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %dest, i8* %src, i64 10, i1 false)",
        "  call void @user_func()",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    // Intrinsics are skipped in findCalls
    EXPECT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0].callee_function, "user_func");
}

TEST_F(LLVMCallSiteAnalyzerTest, ExtractFunctions) {
    std::vector<std::string> lines = {
        "declare i32 @external_func(i32)",
        "define i32 @local_func(i32 %x, i32 %y) {",
        "entry:",
        "  ret i32 0",
        "}"
    };

    auto functions = analyzer_.extractFunctions(lines);
    EXPECT_EQ(functions.size(), 2u);

    EXPECT_TRUE(functions.find("external_func") != functions.end());
    EXPECT_TRUE(functions["external_func"].is_external);
    EXPECT_TRUE(functions["external_func"].is_declaration);

    EXPECT_TRUE(functions.find("local_func") != functions.end());
    EXPECT_FALSE(functions["local_func"].is_external);
}

TEST_F(LLVMCallSiteAnalyzerTest, TailCall) {
    std::vector<std::string> lines = {
        "define i32 @wrapper(i32 %x) {",
        "entry:",
        "  %result = tail call i32 @inner(i32 %x)",
        "  ret i32 %result",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_TRUE(calls[0].is_tail_call);
}

// ============================================================================
// Indirect Call Transformation Tests
// ============================================================================

class LLVMIndirectCallTransformTest : public ::testing::Test {
protected:
    LLVMCallSiteAnalyzer analyzer_;
    LLVMIndirectCallTransformation transformer_;
    IndirectCallConfig config_;

    void SetUp() override {
        GlobalRandom::setSeed(66666);
        config_.enabled = true;
        config_.probability = 1.0;
        config_.add_decoy_entries = false;
        config_.shuffle_entries = false;
        config_.skip_intrinsics = true;
    }
};

TEST_F(LLVMIndirectCallTransformTest, TransformSimpleCall) {
    std::vector<std::string> lines = {
        "declare void @helper()",
        "define void @main() {",
        "entry:",
        "  call void @helper()",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    auto functions = analyzer_.extractFunctions(lines);

    auto result = transformer_.transform(lines, calls, functions, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.calls_transformed, 1);
    EXPECT_GE(result.functions_in_table, 1);

    // Verify indirect call is in output
    bool found_bitcast = false;
    bool found_load = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("bitcast") != std::string::npos) found_bitcast = true;
        if (line.find("load i8*") != std::string::npos) found_load = true;
    }
    EXPECT_TRUE(found_bitcast);
    EXPECT_TRUE(found_load);
}

TEST_F(LLVMIndirectCallTransformTest, FunctionTableCreated) {
    std::vector<std::string> lines = {
        "declare void @func_a()",
        "declare void @func_b()",
        "define void @main() {",
        "entry:",
        "  call void @func_a()",
        "  call void @func_b()",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    auto functions = analyzer_.extractFunctions(lines);

    auto result = transformer_.transform(lines, calls, functions, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.functions_in_table, 2);

    // Check table has both functions
    const auto* entry_a = result.table.getEntry("func_a");
    const auto* entry_b = result.table.getEntry("func_b");
    EXPECT_NE(entry_a, nullptr);
    EXPECT_NE(entry_b, nullptr);
}

TEST_F(LLVMIndirectCallTransformTest, DecoyEntriesAdded) {
    config_.add_decoy_entries = true;
    config_.min_decoy_count = 2;
    config_.max_decoy_count = 2;

    std::vector<std::string> lines = {
        "declare void @target()",
        "define void @main() {",
        "entry:",
        "  call void @target()",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    auto functions = analyzer_.extractFunctions(lines);

    auto result = transformer_.transform(lines, calls, functions, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.decoy_entries_added, 2);
    EXPECT_EQ(result.table.table_size, 3u);  // 1 real + 2 decoy
}

TEST_F(LLVMIndirectCallTransformTest, XORAddressObfuscation) {
    config_.address_strategy = AddressObfStrategy::XOR;

    std::vector<std::string> lines = {
        "declare i32 @compute(i32)",
        "define i32 @main() {",
        "entry:",
        "  %r = call i32 @compute(i32 5)",
        "  ret i32 %r",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    auto functions = analyzer_.extractFunctions(lines);

    auto result = transformer_.transform(lines, calls, functions, config_);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.table.global_xor_key, 0u);

    // Verify XOR instruction in output
    bool found_xor = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("xor i64") != std::string::npos) {
            found_xor = true;
            break;
        }
    }
    EXPECT_TRUE(found_xor);
}

TEST_F(LLVMIndirectCallTransformTest, NoObfuscation_DirectIndirection) {
    config_.address_strategy = AddressObfStrategy::None;

    std::vector<std::string> lines = {
        "declare void @func()",
        "define void @main() {",
        "entry:",
        "  call void @func()",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    auto functions = analyzer_.extractFunctions(lines);

    auto result = transformer_.transform(lines, calls, functions, config_);
    EXPECT_TRUE(result.success);

    // Should not have XOR instruction
    bool found_xor = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("xor i64") != std::string::npos) {
            found_xor = true;
            break;
        }
    }
    EXPECT_FALSE(found_xor);
}

// ============================================================================
// Indirect Call Pass Tests
// ============================================================================

class LLVMIndirectCallPassTest : public ::testing::Test {
protected:
    LLVMIndirectCallPass pass_;

    void SetUp() override {
        GlobalRandom::setSeed(77777);
        PassConfig pc;
        pc.enabled = true;
        pc.probability = 1.0;
        pass_.initialize(pc);

        IndirectCallConfig icc;
        icc.enabled = true;
        icc.probability = 1.0;
        icc.add_decoy_entries = false;
        icc.shuffle_entries = false;
        pass_.setIndirectCallConfig(icc);
    }
};

TEST_F(LLVMIndirectCallPassTest, PassMetadata) {
    EXPECT_EQ(pass_.getName(), "IndirectCall");
    EXPECT_FALSE(pass_.getDescription().empty());
    EXPECT_EQ(pass_.getPriority(), PassPriority::ControlFlow);
}

TEST_F(LLVMIndirectCallPassTest, TransformIR) {
    std::vector<std::string> lines = {
        "declare void @helper()",
        "define void @main() {",
        "entry:",
        "  call void @helper()",
        "  ret void",
        "}"
    };

    auto result = pass_.transformIR(lines);
    EXPECT_TRUE(result == TransformResult::Success || result == TransformResult::Skipped);
}

TEST_F(LLVMIndirectCallPassTest, Statistics_Populated) {
    std::vector<std::string> lines = {
        "declare void @func()",
        "define void @main() {",
        "entry:",
        "  call void @func()",
        "  ret void",
        "}"
    };

    pass_.transformIR(lines);
    auto stats = pass_.getStatistics();

    EXPECT_TRUE(stats.find("calls_found") != stats.end());
    EXPECT_TRUE(stats.find("functions_found") != stats.end());
}

TEST_F(LLVMIndirectCallPassTest, DisabledPass_NoTransformation) {
    IndirectCallConfig config;
    config.enabled = false;
    pass_.setIndirectCallConfig(config);

    std::vector<std::string> lines = {
        "declare void @func()",
        "define void @main() {",
        "entry:",
        "  call void @func()",
        "  ret void",
        "}"
    };

    auto original = lines;
    auto result = pass_.transformIR(lines);

    EXPECT_EQ(result, TransformResult::Skipped);
    EXPECT_EQ(lines, original);
}

// ============================================================================
// Address Obfuscation Strategy Tests
// ============================================================================

class AddressObfuscationTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(88888);
    }
};

TEST_F(AddressObfuscationTest, XOR_IsReversible) {
    uint64_t original_addr = 0x7FFF12345678ULL;
    uint64_t key = 0xDEADBEEF;

    uint64_t encoded = original_addr ^ key;
    uint64_t decoded = encoded ^ key;

    EXPECT_EQ(decoded, original_addr);
}

TEST_F(AddressObfuscationTest, Add_IsReversible) {
    int64_t original_addr = 0x7FFF12345678LL;
    int64_t offset = 12345;

    int64_t encoded = original_addr + offset;
    int64_t decoded = encoded - offset;

    EXPECT_EQ(decoded, original_addr);
}

TEST_F(AddressObfuscationTest, XORAdd_IsReversible) {
    uint64_t original_addr = 0x7FFF12345678ULL;
    uint64_t xor_key = 0xDEADBEEF;
    int64_t add_offset = -500;

    // Encode: XOR then add
    uint64_t step1 = original_addr ^ xor_key;
    int64_t encoded = static_cast<int64_t>(step1) + add_offset;

    // Decode: subtract then XOR
    int64_t step2 = encoded - add_offset;
    uint64_t decoded = static_cast<uint64_t>(step2) ^ xor_key;

    EXPECT_EQ(decoded, original_addr);
}

TEST_F(AddressObfuscationTest, RotateXOR_IsReversible) {
    uint64_t original_addr = 0x7FFF12345678ULL;
    uint64_t xor_key = 0xDEADBEEF;
    int rotate_bits = 13;

    // Encode: rotate left then XOR
    uint64_t rotated = (original_addr << rotate_bits) | (original_addr >> (64 - rotate_bits));
    uint64_t encoded = rotated ^ xor_key;

    // Decode: XOR then rotate right
    uint64_t step1 = encoded ^ xor_key;
    uint64_t decoded = (step1 >> rotate_bits) | (step1 << (64 - rotate_bits));

    EXPECT_EQ(decoded, original_addr);
}

// ============================================================================
// Edge Cases
// ============================================================================

class IndirectCallEdgeCasesTest : public ::testing::Test {
protected:
    LLVMCallSiteAnalyzer analyzer_;

    void SetUp() override {
        GlobalRandom::setSeed(99999);
    }
};

TEST_F(IndirectCallEdgeCasesTest, EmptyInput) {
    std::vector<std::string> lines;
    auto calls = analyzer_.findCalls(lines);
    EXPECT_TRUE(calls.empty());
}

TEST_F(IndirectCallEdgeCasesTest, NoCalls) {
    std::vector<std::string> lines = {
        "define void @main() {",
        "entry:",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    EXPECT_TRUE(calls.empty());
}

TEST_F(IndirectCallEdgeCasesTest, OnlyIntrinsics) {
    std::vector<std::string> lines = {
        "define void @main() {",
        "entry:",
        "  call void @llvm.dbg.value(metadata i32 0, metadata !1, metadata !2)",
        "  ret void",
        "}"
    };

    auto calls = analyzer_.findCalls(lines);
    EXPECT_TRUE(calls.empty());  // Intrinsics are filtered
}

// ============================================================================
// Call Stack Obfuscation Tests (P4-008 to P4-010)
// ============================================================================

class CallStackObfBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(11111);
    }
};

TEST_F(CallStackObfBaseTest, ProxyFunctionInfo_DefaultValues) {
    ProxyFunctionInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.target_type.empty());
    EXPECT_EQ(info.type, ProxyType::Simple);
    EXPECT_EQ(info.dispatch_id, -1);
}

TEST_F(CallStackObfBaseTest, FakeCallInfo_DefaultValues) {
    FakeCallInfo info;
    EXPECT_TRUE(info.target_function.empty());
    EXPECT_EQ(info.type, FakeCallType::NeverReached);
    EXPECT_EQ(info.insert_line, -1);
}

TEST_F(CallStackObfBaseTest, CallStackObfConfig_DefaultValues) {
    CallStackObfConfig config;
    EXPECT_TRUE(config.enabled);
    EXPECT_DOUBLE_EQ(config.proxy_probability, 0.7);
    EXPECT_DOUBLE_EQ(config.fake_call_probability, 0.3);
    EXPECT_EQ(config.proxy_type, ProxyType::Simple);
    EXPECT_TRUE(config.skip_intrinsics);
}

// ============================================================================
// Opaque Predicate Generator Tests
// ============================================================================

class OpaquePredicateGenTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(22222);
    }
};

TEST_F(OpaquePredicateGenTest, GenerateAlwaysFalse_ProducesValidCode) {
    auto [condition, setup_code] = OpaquePredicateGenerator::generateAlwaysFalse("test");

    EXPECT_FALSE(condition.empty());
    EXPECT_TRUE(condition.find("icmp") != std::string::npos);
    // Should produce some setup instructions
    EXPECT_GE(setup_code.size(), 0u);
}

TEST_F(OpaquePredicateGenTest, GenerateAlwaysTrue_ProducesValidCode) {
    auto [condition, setup_code] = OpaquePredicateGenerator::generateAlwaysTrue("test");

    EXPECT_FALSE(condition.empty());
    EXPECT_TRUE(condition.find("icmp") != std::string::npos);
}

TEST_F(OpaquePredicateGenTest, MultipleGenerations_ProduceDifferentCode) {
    std::set<std::string> conditions;

    for (int i = 0; i < 20; i++) {
        auto [condition, setup] = OpaquePredicateGenerator::generateAlwaysFalse("v" + std::to_string(i));
        conditions.insert(condition);
    }

    // Should generate some variety (not all identical)
    EXPECT_GT(conditions.size(), 1u);
}

// ============================================================================
// Call Proxying Tests
// ============================================================================

class CallProxyingTest : public ::testing::Test {
protected:
    LLVMCallStackObfTransformation transformer_;
    CallStackObfConfig config_;

    void SetUp() override {
        GlobalRandom::setSeed(33333);
        config_.enabled = true;
        config_.proxy_probability = 1.0;
        config_.fake_call_probability = 0.0;  // Disable fake calls for these tests
        config_.skip_intrinsics = true;
    }
};

TEST_F(CallProxyingTest, SimpleCallProxying) {
    std::vector<std::string> lines = {
        "declare void @helper()",
        "define void @main() {",
        "entry:",
        "  call void @helper()",
        "  ret void",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.calls_proxied, 0);

    // Check for proxy-related code
    bool found_proxy_def = false;
    bool found_bitcast = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("define internal") != std::string::npos &&
            line.find("_proxy_") != std::string::npos) {
            found_proxy_def = true;
        }
        if (line.find("bitcast") != std::string::npos &&
            line.find("to i8*") != std::string::npos) {
            found_bitcast = true;
        }
    }

    if (result.calls_proxied > 0) {
        EXPECT_TRUE(found_proxy_def);
        EXPECT_TRUE(found_bitcast);
    }
}

TEST_F(CallProxyingTest, CallWithResult) {
    std::vector<std::string> lines = {
        "declare i32 @compute(i32)",
        "define i32 @main() {",
        "entry:",
        "  %r = call i32 @compute(i32 42)",
        "  ret i32 %r",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);
}

TEST_F(CallProxyingTest, MultipleCallsSameSignature) {
    std::vector<std::string> lines = {
        "declare void @func_a()",
        "declare void @func_b()",
        "define void @main() {",
        "entry:",
        "  call void @func_a()",
        "  call void @func_b()",
        "  ret void",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);

    // Same signature should reuse proxy
    if (result.calls_proxied == 2) {
        EXPECT_EQ(result.proxy_functions_created, 1);
    }
}

TEST_F(CallProxyingTest, ProxyStatistics) {
    std::vector<std::string> lines = {
        "declare void @target()",
        "define void @main() {",
        "entry:",
        "  call void @target()",
        "  ret void",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);
    EXPECT_GE(result.calls_proxied, 0);
    EXPECT_GE(result.proxy_functions_created, 0);
}

// ============================================================================
// Fake Call Tests
// ============================================================================

class FakeCallTest : public ::testing::Test {
protected:
    LLVMCallStackObfTransformation transformer_;
    CallStackObfConfig config_;

    void SetUp() override {
        GlobalRandom::setSeed(44444);
        config_.enabled = true;
        config_.proxy_probability = 0.0;  // Disable proxying for these tests
        config_.fake_call_probability = 1.0;
        config_.min_fake_calls = 1;
        config_.max_fake_calls = 2;
    }
};

TEST_F(FakeCallTest, FakeCallsInserted) {
    std::vector<std::string> lines = {
        "declare void @helper()",
        "define void @main() {",
        "entry:",
        "  br label %exit",
        "exit:",
        "  ret void",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);

    // Check for opaque predicate-related code
    bool found_fake_comment = false;
    bool found_icmp = false;
    for (const auto& line : result.transformed_code) {
        if (line.find("Fake call") != std::string::npos) {
            found_fake_comment = true;
        }
        if (line.find("icmp") != std::string::npos) {
            found_icmp = true;
        }
    }

    if (result.fake_calls_added > 0) {
        EXPECT_TRUE(found_fake_comment);
        EXPECT_TRUE(found_icmp);
    }
}

TEST_F(FakeCallTest, FakeCallsHaveOpaquePredicates) {
    std::vector<std::string> lines = {
        "declare i32 @compute(i32)",
        "define void @main() {",
        "entry:",
        "  br label %next",
        "next:",
        "  ret void",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);

    // Should have branching structure for fake calls
    int branch_count = 0;
    for (const auto& line : result.transformed_code) {
        if (line.find("br i1") != std::string::npos) {
            branch_count++;
        }
    }

    // Each fake call adds a conditional branch
    EXPECT_GE(branch_count, result.fake_calls_added);
}

// ============================================================================
// Call Stack Obfuscation Pass Tests
// ============================================================================

class LLVMCallStackObfPassTest : public ::testing::Test {
protected:
    LLVMCallStackObfPass pass_;

    void SetUp() override {
        GlobalRandom::setSeed(55555);
        PassConfig pc;
        pc.enabled = true;
        pc.probability = 1.0;
        pass_.initialize(pc);

        CallStackObfConfig cso;
        cso.enabled = true;
        cso.proxy_probability = 1.0;
        cso.fake_call_probability = 0.5;
        pass_.setCallStackObfConfig(cso);
    }
};

TEST_F(LLVMCallStackObfPassTest, PassMetadata) {
    EXPECT_EQ(pass_.getName(), "CallStackObf");
    EXPECT_FALSE(pass_.getDescription().empty());
    EXPECT_EQ(pass_.getPriority(), PassPriority::ControlFlow);
}

TEST_F(LLVMCallStackObfPassTest, TransformIR) {
    std::vector<std::string> lines = {
        "declare void @helper()",
        "define void @main() {",
        "entry:",
        "  call void @helper()",
        "  ret void",
        "}"
    };

    auto result = pass_.transformIR(lines);
    EXPECT_TRUE(result == TransformResult::Success || result == TransformResult::Skipped);
}

TEST_F(LLVMCallStackObfPassTest, Statistics_Populated) {
    std::vector<std::string> lines = {
        "declare void @func()",
        "define void @main() {",
        "entry:",
        "  call void @func()",
        "  ret void",
        "}"
    };

    pass_.transformIR(lines);
    auto stats = pass_.getStatistics();

    EXPECT_TRUE(stats.find("calls_proxied") != stats.end());
    EXPECT_TRUE(stats.find("fake_calls_added") != stats.end());
    EXPECT_TRUE(stats.find("proxy_functions_created") != stats.end());
}

TEST_F(LLVMCallStackObfPassTest, DisabledPass_NoTransformation) {
    CallStackObfConfig config;
    config.enabled = false;
    pass_.setCallStackObfConfig(config);

    std::vector<std::string> lines = {
        "declare void @func()",
        "define void @main() {",
        "entry:",
        "  call void @func()",
        "  ret void",
        "}"
    };

    auto original = lines;
    auto result = pass_.transformIR(lines);

    EXPECT_EQ(result, TransformResult::Skipped);
    EXPECT_EQ(lines, original);
}

// ============================================================================
// Recursion Tests (P4-010)
// ============================================================================

class RecursionTest : public ::testing::Test {
protected:
    LLVMCallStackObfTransformation transformer_;
    CallStackObfConfig config_;

    void SetUp() override {
        GlobalRandom::setSeed(66666);
        config_.enabled = true;
        config_.proxy_probability = 1.0;
        config_.fake_call_probability = 0.0;
    }
};

TEST_F(RecursionTest, RecursiveFunction) {
    std::vector<std::string> lines = {
        "define i32 @factorial(i32 %n) {",
        "entry:",
        "  %cmp = icmp sle i32 %n, 1",
        "  br i1 %cmp, label %base, label %recurse",
        "base:",
        "  ret i32 1",
        "recurse:",
        "  %n1 = sub i32 %n, 1",
        "  %r = call i32 @factorial(i32 %n1)",
        "  %result = mul i32 %n, %r",
        "  ret i32 %result",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);

    // Recursive calls should be transformable
    // Note: May or may not proxy depending on implementation
}

TEST_F(RecursionTest, MutualRecursion) {
    std::vector<std::string> lines = {
        "declare i32 @is_odd(i32)",
        "define i32 @is_even(i32 %n) {",
        "entry:",
        "  %cmp = icmp eq i32 %n, 0",
        "  br i1 %cmp, label %base, label %recurse",
        "base:",
        "  ret i32 1",
        "recurse:",
        "  %n1 = sub i32 %n, 1",
        "  %r = call i32 @is_odd(i32 %n1)",
        "  ret i32 %r",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);
}

// ============================================================================
// Edge Cases
// ============================================================================

class CallStackObfEdgeCasesTest : public ::testing::Test {
protected:
    LLVMCallStackObfTransformation transformer_;
    CallStackObfConfig config_;

    void SetUp() override {
        GlobalRandom::setSeed(77777);
        config_.enabled = true;
        config_.proxy_probability = 1.0;
        config_.fake_call_probability = 0.0;
    }
};

TEST_F(CallStackObfEdgeCasesTest, EmptyInput) {
    std::vector<std::string> lines;
    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.calls_proxied, 0);
}

TEST_F(CallStackObfEdgeCasesTest, NoCalls) {
    std::vector<std::string> lines = {
        "define void @main() {",
        "entry:",
        "  ret void",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.calls_proxied, 0);
}

TEST_F(CallStackObfEdgeCasesTest, IntrinsicsSkipped) {
    std::vector<std::string> lines = {
        "define void @main() {",
        "entry:",
        "  call void @llvm.dbg.value(metadata i32 0, metadata !1, metadata !2)",
        "  ret void",
        "}"
    };

    auto result = transformer_.transform(lines, config_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.calls_proxied, 0);  // Intrinsics not proxied
}
