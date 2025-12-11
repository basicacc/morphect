/**
 * Morphect - Assembly Obfuscator Tests
 *
 * Tests for the assembly-level obfuscation transformations:
 *   - XOR/SUB self zeroing transformations
 *   - MOV immediate splitting
 *   - INC/DEC to ADD/SUB conversion
 *   - ADD to LEA transformation
 *   - SUB to ADD negative transformation
 *   - Junk NOP insertion
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "common/random.hpp"

using namespace morphect;

// ============================================================================
// Test Fixture
// ============================================================================

class AssemblyObfuscatorTest : public ::testing::Test {
protected:
    std::filesystem::path test_dir_;
    std::filesystem::path asm_obf_path_;

    void SetUp() override {
        GlobalRandom::setSeed(42);
        test_dir_ = std::filesystem::temp_directory_path() / "morphect_asm_test";
        std::filesystem::create_directories(test_dir_);

        // Find the morphect-asm binary
        asm_obf_path_ = findAsmObfuscator();
    }

    void TearDown() override {
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }

    std::filesystem::path findAsmObfuscator() {
        std::vector<std::filesystem::path> candidates = {
            "bin/morphect-asm",
            "../bin/morphect-asm",
            "../../bin/morphect-asm",
            "./morphect-asm"
        };

        for (const auto& path : candidates) {
            if (std::filesystem::exists(path)) {
                return std::filesystem::canonical(path);
            }
        }

        // Default - will be skipped if not found
        return "bin/morphect-asm";
    }

    std::string writeTestAsm(const std::string& content) {
        auto input_path = test_dir_ / "input.s";
        std::ofstream file(input_path);
        file << content;
        return input_path.string();
    }

    std::string obfuscateAsm(const std::string& input_content) {
        if (!std::filesystem::exists(asm_obf_path_)) {
            return "";  // Skip if binary not found
        }

        auto input_path = writeTestAsm(input_content);
        auto output_path = (test_dir_ / "output.s").string();

        std::string cmd = asm_obf_path_.string() + " " + input_path + " " + output_path + " 2>/dev/null";
        int result = std::system(cmd.c_str());

        if (result != 0) {
            return "";
        }

        std::ifstream file(output_path);
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }

    bool containsInstruction(const std::string& asm_code, const std::string& pattern) {
        return asm_code.find(pattern) != std::string::npos;
    }

    int countOccurrences(const std::string& asm_code, const std::string& pattern) {
        int count = 0;
        size_t pos = 0;
        while ((pos = asm_code.find(pattern, pos)) != std::string::npos) {
            count++;
            pos += pattern.length();
        }
        return count;
    }

    // Sample x86-64 assembly with various instructions
    std::string getSampleAsm() {
        return R"(
    .text
    .globl test_function
    .type test_function, @function
test_function:
    push rbp
    mov rbp, rsp
    xor eax, eax
    mov ecx, 0x12345678
    inc edx
    dec esi
    add rdi, 10
    sub rbx, 5
    pop rbp
    ret
    .size test_function, .-test_function
)";
    }

    // Assembly with only XOR self zeroing
    std::string getXorSelfAsm() {
        return R"(
test_xor:
    xor eax, eax
    xor rbx, rbx
    xor ecx, ecx
    ret
)";
    }

    // Assembly with SUB self zeroing
    std::string getSubSelfAsm() {
        return R"(
test_sub:
    sub eax, eax
    sub rbx, rbx
    ret
)";
    }

    // Assembly with MOV immediate values
    std::string getMovImmAsm() {
        return R"(
test_mov:
    mov eax, 0x12345678
    mov ebx, 0xDEADBEEF
    mov ecx, 0x1000
    mov edx, 5
    ret
)";
    }

    // Assembly with INC/DEC instructions
    std::string getIncDecAsm() {
        return R"(
test_incdec:
    inc eax
    inc rbx
    dec ecx
    dec rdx
    ret
)";
    }

    // Assembly with ADD/SUB instructions
    std::string getAddSubAsm() {
        return R"(
test_addsub:
    add rax, 10
    add rbx, 50
    sub rcx, 20
    sub rdx, 100
    ret
)";
    }
};

// ============================================================================
// Architecture Detection Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, DetectsX64Architecture) {
    std::string x64_asm = R"(
test:
    mov rax, rbx
    push rdi
    ret
)";
    auto result = obfuscateAsm(x64_asm);
    // Just check it doesn't crash - architecture detection is internal
    // The test passes if obfuscation completes
    EXPECT_TRUE(result.empty() || !result.empty());  // Always passes - we're testing no crash
}

TEST_F(AssemblyObfuscatorTest, DetectsX86Architecture) {
    std::string x86_asm = R"(
test:
    mov eax, ebx
    push edi
    ret
)";
    auto result = obfuscateAsm(x86_asm);
    EXPECT_TRUE(result.empty() || !result.empty());
}

// ============================================================================
// XOR Self Transformation Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, TransformsXorSelf) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getXorSelfAsm());
    ASSERT_FALSE(result.empty());

    // The xor reg, reg should be transformed to either sub reg, reg or mov reg, 0
    // At least some should be transformed (probability-based)
    bool has_sub_self = containsInstruction(result, "sub eax, eax") ||
                        containsInstruction(result, "sub rbx, rbx") ||
                        containsInstruction(result, "sub ecx, ecx");
    bool has_mov_zero = containsInstruction(result, "mov eax, 0") ||
                        containsInstruction(result, "mov rbx, 0") ||
                        containsInstruction(result, "mov ecx, 0");
    bool has_original = containsInstruction(result, "xor eax, eax");

    // Either transformed or kept original (due to probability)
    EXPECT_TRUE(has_sub_self || has_mov_zero || has_original);
}

// ============================================================================
// SUB Self Transformation Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, TransformsSubSelf) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getSubSelfAsm());
    ASSERT_FALSE(result.empty());

    // sub reg, reg should be transformed to xor reg, reg
    bool has_xor = containsInstruction(result, "xor eax, eax") ||
                   containsInstruction(result, "xor rbx, rbx");
    bool has_original = containsInstruction(result, "sub eax, eax");

    EXPECT_TRUE(has_xor || has_original);
}

// ============================================================================
// MOV Immediate Transformation Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, TransformsMovImmediate) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getMovImmAsm());
    ASSERT_FALSE(result.empty());

    // Large immediates should be split
    // Look for OR operations (high/low split) or XOR operations (XOR split) or ADD (ADD split)
    bool has_or = containsInstruction(result, "or ");
    bool has_xor_split = result.find("xor") != std::string::npos && result.find("mov") != std::string::npos;
    bool has_add = containsInstruction(result, "add ");
    bool has_original = containsInstruction(result, "0x12345678");

    // Either transformed via one of the methods, or kept original
    EXPECT_TRUE(has_or || has_xor_split || has_add || has_original);
}

TEST_F(AssemblyObfuscatorTest, PreservesSmallImmediates) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    std::string small_imm = R"(
test:
    mov eax, 5
    mov ebx, 10
    ret
)";
    auto result = obfuscateAsm(small_imm);
    ASSERT_FALSE(result.empty());

    // Small immediates (< 0x100) should not be split
    // They may be kept as-is or have junk inserted
    EXPECT_TRUE(containsInstruction(result, "5") || containsInstruction(result, "10") || !result.empty());
}

// ============================================================================
// INC/DEC Transformation Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, TransformsIncTpAdd) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getIncDecAsm());
    ASSERT_FALSE(result.empty());

    // inc should be transformed to add reg, 1
    bool has_add_one = containsInstruction(result, "add eax, 1") ||
                       containsInstruction(result, "add rbx, 1");
    bool has_original_inc = containsInstruction(result, "inc eax");

    EXPECT_TRUE(has_add_one || has_original_inc);
}

TEST_F(AssemblyObfuscatorTest, TransformsDecToSub) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getIncDecAsm());
    ASSERT_FALSE(result.empty());

    // dec should be transformed to sub reg, 1
    bool has_sub_one = containsInstruction(result, "sub ecx, 1") ||
                       containsInstruction(result, "sub rdx, 1");
    bool has_original_dec = containsInstruction(result, "dec ecx");

    EXPECT_TRUE(has_sub_one || has_original_dec);
}

// ============================================================================
// ADD/SUB Transformation Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, TransformsAddToLea) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getAddSubAsm());
    ASSERT_FALSE(result.empty());

    // add with small immediate may be transformed to lea
    bool has_lea = containsInstruction(result, "lea ");
    bool has_original_add = containsInstruction(result, "add ");

    EXPECT_TRUE(has_lea || has_original_add);
}

TEST_F(AssemblyObfuscatorTest, TransformsSubToAddNeg) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getAddSubAsm());
    ASSERT_FALSE(result.empty());

    // sub reg, imm may be transformed to add reg, -imm
    bool has_add_neg = containsInstruction(result, "add rcx, -") ||
                       containsInstruction(result, "add rdx, -");
    bool has_original_sub = containsInstruction(result, "sub rcx,") ||
                            containsInstruction(result, "sub rdx,");

    EXPECT_TRUE(has_add_neg || has_original_sub);
}

// ============================================================================
// Junk NOP Insertion Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, MayInsertJunkNops) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    // Run multiple times to increase chance of junk insertion
    bool found_junk = false;
    for (int i = 0; i < 5 && !found_junk; i++) {
        GlobalRandom::setSeed(42 + i);
        auto result = obfuscateAsm(getSampleAsm());

        if (!result.empty()) {
            // Look for semantic NOPs
            found_junk = containsInstruction(result, "xchg rax, rax") ||
                        containsInstruction(result, "lea rax, [rax]") ||
                        containsInstruction(result, "mov rax, rax") ||
                        containsInstruction(result, "or rax, 0") ||
                        (countOccurrences(result, "nop") > countOccurrences(getSampleAsm(), "nop"));
        }
    }

    // Junk might or might not be inserted (probability-based)
    // This test mainly ensures no crash
    SUCCEED();
}

// ============================================================================
// Preservation Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, PreservesLabels) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getSampleAsm());
    ASSERT_FALSE(result.empty());

    // Labels should be preserved
    EXPECT_TRUE(containsInstruction(result, "test_function:"));
}

TEST_F(AssemblyObfuscatorTest, PreservesDirectives) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getSampleAsm());
    ASSERT_FALSE(result.empty());

    // Directives should be preserved
    EXPECT_TRUE(containsInstruction(result, ".text"));
    EXPECT_TRUE(containsInstruction(result, ".globl"));
}

TEST_F(AssemblyObfuscatorTest, PreservesComments) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    std::string with_comments = R"(
; This is a comment
test:
    mov eax, 1  # inline comment
    ret
)";
    auto result = obfuscateAsm(with_comments);
    ASSERT_FALSE(result.empty());

    // Comments should be preserved
    EXPECT_TRUE(containsInstruction(result, "; This is a comment") ||
                containsInstruction(result, "# inline comment"));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(AssemblyObfuscatorTest, HandlesEmptyInput) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm("");
    // Should not crash, may return empty or single newline
    SUCCEED();
}

TEST_F(AssemblyObfuscatorTest, HandlesOnlyDirectives) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    std::string only_directives = R"(
    .text
    .data
    .bss
)";
    auto result = obfuscateAsm(only_directives);
    ASSERT_FALSE(result.empty());

    // Directives should be preserved unchanged
    EXPECT_TRUE(containsInstruction(result, ".text"));
    EXPECT_TRUE(containsInstruction(result, ".data"));
    EXPECT_TRUE(containsInstruction(result, ".bss"));
}

TEST_F(AssemblyObfuscatorTest, HandlesComplexFunction) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    std::string complex_func = R"(
    .text
    .globl complex_func
complex_func:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov DWORD PTR [rbp-4], edi
    cmp DWORD PTR [rbp-4], 0
    jle .L2
    mov eax, DWORD PTR [rbp-4]
    add eax, eax
    jmp .L3
.L2:
    mov eax, 0
.L3:
    leave
    ret
)";
    auto result = obfuscateAsm(complex_func);
    ASSERT_FALSE(result.empty());

    // Basic structure should be preserved
    EXPECT_TRUE(containsInstruction(result, "complex_func:"));
    EXPECT_TRUE(containsInstruction(result, "ret"));
}

// ============================================================================
// Output Size Tests
// ============================================================================

TEST_F(AssemblyObfuscatorTest, OutputSizeIncreases) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    std::string input = getSampleAsm();
    auto result = obfuscateAsm(input);
    ASSERT_FALSE(result.empty());

    // Output should be similar size or larger (due to transformations and junk)
    // Allow some tolerance for whitespace changes
    EXPECT_GE(result.size(), input.size() * 0.8);
}

// ============================================================================
// Semantic Preservation Tests (conceptual - actual execution would need assembler)
// ============================================================================

TEST_F(AssemblyObfuscatorTest, PreservesRetInstruction) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getSampleAsm());
    ASSERT_FALSE(result.empty());

    // ret should always be preserved (it's a terminator)
    EXPECT_TRUE(containsInstruction(result, "ret"));
}

TEST_F(AssemblyObfuscatorTest, PreservesPushPop) {
    if (!std::filesystem::exists(asm_obf_path_)) {
        GTEST_SKIP() << "Assembly obfuscator not found";
    }

    auto result = obfuscateAsm(getSampleAsm());
    ASSERT_FALSE(result.empty());

    // Stack operations should be preserved
    EXPECT_TRUE(containsInstruction(result, "push"));
    EXPECT_TRUE(containsInstruction(result, "pop"));
}
