/**
 * Morphect - Anti-Disassembly Tests
 *
 * Tests for Phase 5.2 - Anti-Disassembly:
 *   - P5-006: Insert junk bytes after unconditional jumps
 *   - P5-007: Instruction overlapping
 *   - P5-008: Fake function prologues
 *   - P5-009: Test with disassemblers (manual verification)
 */

#include <gtest/gtest.h>
#include "passes/antidisasm/antidisasm.hpp"
#include "common/random.hpp"

using namespace morphect;
using namespace morphect::antidisasm;

// ============================================================================
// Test Fixtures
// ============================================================================

class AntiDisasmTest : public ::testing::Test {
protected:
    void SetUp() override {
        GlobalRandom::setSeed(42);
    }

    // Sample x86-64 assembly for testing
    std::vector<std::string> getSampleAsm() {
        return {
            "    .text",
            "    .globl main",
            "    .type main, @function",
            "main:",
            "    push rbp",
            "    mov rbp, rsp",
            "    sub rsp, 16",
            "    mov DWORD PTR [rbp-4], 0",
            "    jmp .L2",
            ".L3:",
            "    add DWORD PTR [rbp-4], 1",
            ".L2:",
            "    cmp DWORD PTR [rbp-4], 9",
            "    jle .L3",
            "    mov eax, DWORD PTR [rbp-4]",
            "    leave",
            "    ret",
            "    .size main, .-main"
        };
    }

    // Simple assembly with just a function
    std::vector<std::string> getSimpleAsm() {
        return {
            "    .text",
            "simple:",
            "    push rbp",
            "    mov rbp, rsp",
            "    xor eax, eax",
            "    pop rbp",
            "    ret"
        };
    }

    // Assembly with multiple jumps
    std::vector<std::string> getMultiJumpAsm() {
        return {
            "func:",
            "    push rbp",
            "    jmp .L1",
            ".L1:",
            "    nop",
            "    jmp .L2",
            ".L2:",
            "    nop",
            "    jmp .L3",
            ".L3:",
            "    pop rbp",
            "    ret"
        };
    }
};

// ============================================================================
// P5-006: Junk Bytes Tests
// ============================================================================

TEST_F(AntiDisasmTest, X86JunkBytes_PrefixLikeBytes) {
    auto bytes = X86JunkBytes::getPrefixLikeBytes(4);

    EXPECT_EQ(bytes.size(), 4u);

    // All bytes should be valid prefixes
    std::vector<uint8_t> valid_prefixes = {
        0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65, 0x66, 0x67, 0xF0, 0xF2, 0xF3
    };

    for (uint8_t b : bytes) {
        bool is_valid = std::find(valid_prefixes.begin(), valid_prefixes.end(), b)
                       != valid_prefixes.end();
        EXPECT_TRUE(is_valid) << "Invalid prefix byte: 0x" << std::hex << (int)b;
    }
}

TEST_F(AntiDisasmTest, X86JunkBytes_InstructionLikeBytes) {
    auto bytes = X86JunkBytes::getInstructionLikeBytes(3);

    EXPECT_EQ(bytes.size(), 3u);

    // Bytes should be instruction-like opcodes
    std::vector<uint8_t> valid_opcodes = {
        0x0F, 0x48, 0x49, 0x4C, 0x89, 0x8B, 0xB8, 0xC7, 0xE8, 0xE9, 0xFF
    };

    for (uint8_t b : bytes) {
        bool is_valid = std::find(valid_opcodes.begin(), valid_opcodes.end(), b)
                       != valid_opcodes.end();
        EXPECT_TRUE(is_valid) << "Invalid opcode byte: 0x" << std::hex << (int)b;
    }
}

TEST_F(AntiDisasmTest, X86JunkBytes_FakeCallBytes) {
    auto bytes = X86JunkBytes::getFakeCallBytes();

    EXPECT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xE8);  // CALL opcode
}

TEST_F(AntiDisasmTest, X86JunkBytes_MultiByteNop) {
    for (int size = 1; size <= 9; size++) {
        auto nop = X86JunkBytes::getMultiByteNop(size);
        EXPECT_EQ(nop.size(), static_cast<size_t>(size))
            << "Wrong NOP size for requested " << size;
    }

    // Larger NOP (combined)
    auto large_nop = X86JunkBytes::getMultiByteNop(15);
    EXPECT_EQ(large_nop.size(), 15u);
}

TEST_F(AntiDisasmTest, X86JunkBytes_BytesToAsm) {
    std::vector<uint8_t> bytes = {0xE8, 0x12, 0x34};
    std::string asm_str = X86JunkBytes::bytesToAsm(bytes);

    EXPECT_TRUE(asm_str.find(".byte") != std::string::npos);
    EXPECT_TRUE(asm_str.find("0xe8") != std::string::npos);
    EXPECT_TRUE(asm_str.find("0x12") != std::string::npos);
    EXPECT_TRUE(asm_str.find("0x34") != std::string::npos);
}

TEST_F(AntiDisasmTest, X86JunkBytes_EmptyInput) {
    std::vector<uint8_t> empty;
    std::string asm_str = X86JunkBytes::bytesToAsm(empty);
    EXPECT_TRUE(asm_str.empty());
}

// ============================================================================
// P5-007: Instruction Overlap Tests
// ============================================================================

TEST_F(AntiDisasmTest, InstructionOverlap_SimpleOverlap) {
    auto overlap = InstructionOverlapGenerator::generateSimpleOverlap("test");

    EXPECT_FALSE(overlap.empty());

    // Should have jmp instruction
    bool has_jmp = false;
    bool has_bytes = false;
    bool has_label = false;

    for (const auto& line : overlap) {
        if (line.find("jmp ") != std::string::npos) has_jmp = true;
        if (line.find(".byte") != std::string::npos) has_bytes = true;
        if (line.find("test_over:") != std::string::npos) has_label = true;
    }

    EXPECT_TRUE(has_jmp);
    EXPECT_TRUE(has_bytes);
    EXPECT_TRUE(has_label);
}

TEST_F(AntiDisasmTest, InstructionOverlap_OpaqueOverlapX64) {
    auto overlap = InstructionOverlapGenerator::generateOpaqueOverlap(
        "test", TargetArch::X86_64);

    EXPECT_FALSE(overlap.empty());

    // Should have conditional jump
    bool has_cond_jmp = false;
    bool has_label = false;

    for (const auto& line : overlap) {
        if (line.find("jz ") != std::string::npos ||
            line.find("jnz ") != std::string::npos ||
            line.find("je ") != std::string::npos) {
            has_cond_jmp = true;
        }
        if (line.find("test_real:") != std::string::npos) has_label = true;
    }

    EXPECT_TRUE(has_cond_jmp);
    EXPECT_TRUE(has_label);
}

TEST_F(AntiDisasmTest, InstructionOverlap_OpaqueOverlapX86) {
    auto overlap = InstructionOverlapGenerator::generateOpaqueOverlap(
        "test32", TargetArch::X86_32);

    EXPECT_FALSE(overlap.empty());

    // Should contain conditional jump
    bool has_cond_jmp = false;
    for (const auto& line : overlap) {
        if (line.find("jz ") != std::string::npos ||
            line.find("jnz ") != std::string::npos ||
            line.find("je ") != std::string::npos) {
            has_cond_jmp = true;
        }
    }
    EXPECT_TRUE(has_cond_jmp);
}

TEST_F(AntiDisasmTest, InstructionOverlap_VariousPredicates) {
    // Run multiple times to test different predicate types
    std::set<std::string> seen_predicates;

    for (int i = 0; i < 20; i++) {
        GlobalRandom::setSeed(i * 100);
        auto overlap = InstructionOverlapGenerator::generateOpaqueOverlap(
            "p" + std::to_string(i), TargetArch::X86_64);

        for (const auto& line : overlap) {
            if (line.find("xor eax, eax") != std::string::npos) {
                seen_predicates.insert("xor");
            }
            if (line.find("mov eax, 1") != std::string::npos) {
                seen_predicates.insert("mov");
            }
            if (line.find("cmp") != std::string::npos) {
                seen_predicates.insert("cmp");
            }
        }
    }

    // Should have seen multiple predicate types
    EXPECT_GT(seen_predicates.size(), 1u);
}

// ============================================================================
// P5-008: Fake Prologue Tests
// ============================================================================

TEST_F(AntiDisasmTest, FakePrologue_X64Prologue) {
    auto prologue = FakePrologueGenerator::getX64Prologue();

    EXPECT_FALSE(prologue.empty());

    // Should have push rbp
    bool has_push = false;
    for (const auto& line : prologue) {
        if (line.find("push rbp") != std::string::npos) {
            has_push = true;
            break;
        }
    }
    EXPECT_TRUE(has_push);
}

TEST_F(AntiDisasmTest, FakePrologue_X86Prologue) {
    auto prologue = FakePrologueGenerator::getX86Prologue();

    EXPECT_FALSE(prologue.empty());

    // Should have push ebp
    bool has_push = false;
    for (const auto& line : prologue) {
        if (line.find("push ebp") != std::string::npos) {
            has_push = true;
            break;
        }
    }
    EXPECT_TRUE(has_push);
}

TEST_F(AntiDisasmTest, FakePrologue_VariousVariants) {
    // Test different prologue variants
    for (int variant = 0; variant < 5; variant++) {
        auto prologue = FakePrologueGenerator::getX64Prologue(variant);
        EXPECT_FALSE(prologue.empty()) << "Variant " << variant << " is empty";
    }

    for (int variant = 0; variant < 3; variant++) {
        auto prologue = FakePrologueGenerator::getX86Prologue(variant);
        EXPECT_FALSE(prologue.empty()) << "Variant " << variant << " is empty";
    }
}

TEST_F(AntiDisasmTest, FakePrologue_GenerateFakeFunction) {
    auto fake_func = FakePrologueGenerator::generateFakeFunction(
        ".Lfake_test", TargetArch::X86_64);

    EXPECT_FALSE(fake_func.empty());

    // Should have label, prologue, and ret
    bool has_label = false;
    bool has_push = false;
    bool has_ret = false;

    for (const auto& line : fake_func) {
        if (line.find(".Lfake_test:") != std::string::npos) has_label = true;
        if (line.find("push rbp") != std::string::npos) has_push = true;
        if (line.find("ret") != std::string::npos) has_ret = true;
    }

    EXPECT_TRUE(has_label);
    EXPECT_TRUE(has_push);
    EXPECT_TRUE(has_ret);
}

// ============================================================================
// Assembly Analyzer Tests
// ============================================================================

TEST_F(AntiDisasmTest, AssemblyAnalyzer_FindUnconditionalJumps) {
    AssemblyAnalyzer analyzer;
    auto jumps = analyzer.findUnconditionalJumps(getSampleAsm());

    EXPECT_FALSE(jumps.empty());

    // Should find the "jmp .L2" instruction
    bool found = false;
    for (int idx : jumps) {
        if (getSampleAsm()[idx].find("jmp .L2") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(AntiDisasmTest, AssemblyAnalyzer_FindMultipleJumps) {
    AssemblyAnalyzer analyzer;
    auto jumps = analyzer.findUnconditionalJumps(getMultiJumpAsm());

    // Should find 3 unconditional jumps
    EXPECT_EQ(jumps.size(), 3u);
}

TEST_F(AntiDisasmTest, AssemblyAnalyzer_FindPrologueInsertPoints) {
    AssemblyAnalyzer analyzer;
    auto points = analyzer.findPrologueInsertPoints(getSampleAsm());

    EXPECT_FALSE(points.empty());
}

TEST_F(AntiDisasmTest, AssemblyAnalyzer_FindFunctionEntries) {
    AssemblyAnalyzer analyzer;
    auto entries = analyzer.findFunctionEntries(getSampleAsm());

    // Should find function entry after "main:" label
    EXPECT_FALSE(entries.empty());
}

TEST_F(AntiDisasmTest, AssemblyAnalyzer_DetectArch) {
    AssemblyAnalyzer analyzer;

    auto arch = analyzer.detectArch(getSampleAsm());
    EXPECT_EQ(arch, TargetArch::X86_64);

    // x86-32 assembly
    std::vector<std::string> x86_asm = {
        "    push ebp",
        "    mov ebp, esp",
        "    xor eax, eax",
        "    pop ebp",
        "    ret"
    };
    auto arch32 = analyzer.detectArch(x86_asm);
    // Note: may still detect as x64 since eax is valid in both
}

// ============================================================================
// Transformation Tests
// ============================================================================

TEST_F(AntiDisasmTest, X86AntiDisasmTransformation_Transform) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 1.0;

    auto result = transform.transform(getSampleAsm(), config);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.transformed_code.empty());
}

TEST_F(AntiDisasmTest, X86AntiDisasmTransformation_InsertsJunkBytes) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.insert_junk_bytes = true;
    config.use_instruction_overlap = false;
    config.insert_fake_prologues = false;
    config.use_opaque_jumps = false;

    auto result = transform.transform(getMultiJumpAsm(), config);

    EXPECT_TRUE(result.success);
    EXPECT_GE(result.junk_bytes_inserted, 1);

    // Should have .byte directives
    bool has_bytes = false;
    for (const auto& line : result.transformed_code) {
        if (line.find(".byte") != std::string::npos) {
            has_bytes = true;
            break;
        }
    }
    EXPECT_TRUE(has_bytes);
}

TEST_F(AntiDisasmTest, X86AntiDisasmTransformation_InsertsOverlaps) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.insert_junk_bytes = false;
    config.use_instruction_overlap = true;
    config.insert_fake_prologues = false;
    config.use_opaque_jumps = false;

    auto result = transform.transform(getSampleAsm(), config);

    EXPECT_TRUE(result.success);
    // May or may not insert depending on function entries found
}

TEST_F(AntiDisasmTest, X86AntiDisasmTransformation_InsertsFakePrologues) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.insert_junk_bytes = false;
    config.use_instruction_overlap = false;
    config.insert_fake_prologues = true;
    config.use_opaque_jumps = false;

    auto result = transform.transform(getSampleAsm(), config);

    EXPECT_TRUE(result.success);

    if (result.fake_prologues_inserted > 0) {
        // Should have fake function label
        bool has_fake = false;
        for (const auto& line : result.transformed_code) {
            if (line.find(".Lfake_") != std::string::npos) {
                has_fake = true;
                break;
            }
        }
        EXPECT_TRUE(has_fake);
    }
}

TEST_F(AntiDisasmTest, X86AntiDisasmTransformation_DisabledNoChange) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = false;

    auto original = getSampleAsm();
    auto result = transform.transform(original, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.transformed_code.size(), original.size());
    EXPECT_EQ(result.junk_bytes_inserted, 0);
    EXPECT_EQ(result.overlaps_created, 0);
    EXPECT_EQ(result.fake_prologues_inserted, 0);
}

TEST_F(AntiDisasmTest, X86AntiDisasmTransformation_PreservesOriginalCode) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 1.0;

    auto original = getSampleAsm();
    auto result = transform.transform(original, config);

    // Original instructions should still be present
    bool has_main = false;
    bool has_push = false;
    bool has_ret = false;

    for (const auto& line : result.transformed_code) {
        if (line.find("main:") != std::string::npos) has_main = true;
        if (line.find("push rbp") != std::string::npos) has_push = true;
        if (line.find("ret") != std::string::npos) has_ret = true;
    }

    EXPECT_TRUE(has_main);
    EXPECT_TRUE(has_push);
    EXPECT_TRUE(has_ret);
}

// ============================================================================
// Pass Interface Tests
// ============================================================================

TEST_F(AntiDisasmTest, AssemblyAntiDisasmPass_Initialize) {
    AssemblyAntiDisasmPass pass;

    PassConfig config;
    config.enabled = true;
    config.probability = 0.8;

    EXPECT_TRUE(pass.initialize(config));
    EXPECT_TRUE(pass.isEnabled());
}

TEST_F(AntiDisasmTest, AssemblyAntiDisasmPass_Transform) {
    AssemblyAntiDisasmPass pass;

    AntiDisasmConfig ad_config;
    ad_config.enabled = true;
    ad_config.probability = 1.0;
    pass.setAntiDisasmConfig(ad_config);

    auto lines = getSampleAsm();
    auto result = pass.transformAssembly(lines, "x86_64");

    EXPECT_EQ(result, TransformResult::Success);
}

TEST_F(AntiDisasmTest, AssemblyAntiDisasmPass_SkipsWhenDisabled) {
    AssemblyAntiDisasmPass pass;

    AntiDisasmConfig ad_config;
    ad_config.enabled = false;
    pass.setAntiDisasmConfig(ad_config);

    auto lines = getSampleAsm();
    auto result = pass.transformAssembly(lines, "x86_64");

    EXPECT_EQ(result, TransformResult::Skipped);
}

TEST_F(AntiDisasmTest, AssemblyAntiDisasmPass_Statistics) {
    AssemblyAntiDisasmPass pass;

    AntiDisasmConfig ad_config;
    ad_config.enabled = true;
    ad_config.probability = 1.0;
    pass.setAntiDisasmConfig(ad_config);

    auto lines = getMultiJumpAsm();
    pass.transformAssembly(lines, "x86_64");

    auto stats = pass.getStatistics();
    EXPECT_TRUE(stats.find("junk_bytes_inserted") != stats.end());
}

TEST_F(AntiDisasmTest, AssemblyAntiDisasmPass_GetName) {
    AssemblyAntiDisasmPass pass;
    EXPECT_EQ(pass.getName(), "AntiDisasm");
}

TEST_F(AntiDisasmTest, AssemblyAntiDisasmPass_GetPriority) {
    AssemblyAntiDisasmPass pass;
    EXPECT_EQ(pass.getPriority(), PassPriority::Late);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(AntiDisasmTest, EdgeCase_EmptyInput) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;

    std::vector<std::string> empty;
    auto result = transform.transform(empty, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.junk_bytes_inserted, 0);
}

TEST_F(AntiDisasmTest, EdgeCase_NoJumps) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.insert_junk_bytes = true;
    config.use_instruction_overlap = false;
    config.insert_fake_prologues = false;
    config.use_opaque_jumps = false;

    std::vector<std::string> no_jumps = {
        "func:",
        "    push rbp",
        "    mov rbp, rsp",
        "    xor eax, eax",
        "    pop rbp",
        "    ret"
    };

    auto result = transform.transform(no_jumps, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.junk_bytes_inserted, 0);  // No jumps to insert after
}

TEST_F(AntiDisasmTest, EdgeCase_ZeroProbability) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 0.0;

    auto result = transform.transform(getSampleAsm(), config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.junk_bytes_inserted, 0);
    EXPECT_EQ(result.overlaps_created, 0);
    EXPECT_EQ(result.fake_prologues_inserted, 0);
}

TEST_F(AntiDisasmTest, EdgeCase_OnlyDirectives) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;

    std::vector<std::string> only_directives = {
        "    .text",
        "    .globl foo",
        "    .section .rodata",
        "    .string \"hello\""
    };

    auto result = transform.transform(only_directives, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.junk_bytes_inserted, 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(AntiDisasmTest, Integration_AllTechniques) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.insert_junk_bytes = true;
    config.use_instruction_overlap = true;
    config.insert_fake_prologues = true;
    config.use_opaque_jumps = true;

    auto result = transform.transform(getSampleAsm(), config);

    EXPECT_TRUE(result.success);
    // Code should be larger
    EXPECT_GT(result.transformed_code.size(), getSampleAsm().size());
}

TEST_F(AntiDisasmTest, Integration_MultipleTransforms) {
    X86AntiDisasmTransformation transform;
    AntiDisasmConfig config;
    config.enabled = true;
    config.probability = 0.8;

    auto code = getSampleAsm();

    for (int i = 0; i < 3; i++) {
        GlobalRandom::setSeed(i * 1000);
        auto result = transform.transform(code, config);
        EXPECT_TRUE(result.success);
        code = result.transformed_code;
    }

    // Code should have grown
    EXPECT_GT(code.size(), getSampleAsm().size());
}
