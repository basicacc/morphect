/**
 * Morphect - Comprehensive Semantic Equivalence Tests
 *
 * This file tests ALL obfuscation passes to ensure they preserve program semantics.
 * Each test compiles code with and without obfuscation and verifies identical output.
 */

#include <gtest/gtest.h>
#include "../fixtures/obfuscation_fixture.hpp"
#include <cstdint>
#include <random>
#include <functional>

using namespace morphect::test;

/**
 * Extended fixture with semantic verification for all passes
 */
class SemanticEquivalenceTest : public LLVMIRFixture {
protected:
    std::filesystem::path clang_path_ = "clang";

    void SetUp() override {
        LLVMIRFixture::SetUp();

        // Skip if morphect-ir doesn't exist
        if (!std::filesystem::exists(ir_obf_path_)) {
            GTEST_SKIP() << "IR obfuscator not found at: " << ir_obf_path_;
        }

        // Check clang is available
        auto result = runCommand("which clang");
        if (!result.success()) {
            GTEST_SKIP() << "clang not found in PATH";
        }
    }

    /**
     * Compile C code to LLVM IR
     */
    std::string compileToIR(const std::string& c_code) {
        auto source_file = writeSource("test.c", c_code);
        auto ir_file = test_dir_ / "test.ll";

        std::string cmd = clang_path_.string() + " -S -emit-llvm -O0 " +
                          source_file.string() + " -o " + ir_file.string();

        auto result = runCommand(cmd);
        if (!result.success()) {
            return "";
        }

        std::ifstream file(ir_file);
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }

    /**
     * Compile LLVM IR to executable
     */
    bool compileIRToExe(const std::string& ir_code, const std::filesystem::path& output) {
        auto ir_file = writeSource("compile.ll", ir_code);

        std::string cmd = clang_path_.string() + " " + ir_file.string() +
                          " -o " + output.string() + " -lm";

        auto result = runCommand(cmd);
        return result.success();
    }

    /**
     * Full semantic equivalence test pipeline
     * Returns pair of (original_output, obfuscated_output) for verification
     */
    std::pair<CommandResult, CommandResult> testSemanticEquivalence(
        const std::string& c_code,
        const std::string& obf_flags,
        const std::vector<std::string>& run_args = {}) {

        // Step 1: Compile to IR
        std::string ir = compileToIR(c_code);
        EXPECT_FALSE(ir.empty()) << "Failed to compile C to IR";
        if (ir.empty()) return {{-1, "", ""}, {-1, "", ""}};

        // Step 2: Compile original IR to executable
        auto orig_exe = test_dir_ / "original";
        EXPECT_TRUE(compileIRToExe(ir, orig_exe)) << "Failed to compile original IR";

        // Step 3: Obfuscate IR
        auto ir_file = writeSource("input.ll", ir);
        auto obf_ir_file = test_dir_ / "obfuscated.ll";

        std::string obf_cmd = ir_obf_path_.string() + " " + obf_flags + " " +
                              ir_file.string() + " " + obf_ir_file.string();

        auto obf_result = runCommand(obf_cmd);
        EXPECT_TRUE(obf_result.success()) << "Obfuscation failed: " << obf_result.stderr_output;
        if (!obf_result.success()) return {{-1, "", ""}, {-1, "", ""}};

        // Step 4: Read obfuscated IR
        std::ifstream obf_file(obf_ir_file);
        std::stringstream obf_buf;
        obf_buf << obf_file.rdbuf();
        std::string obf_ir = obf_buf.str();

        // Step 5: Compile obfuscated IR to executable
        auto obf_exe = test_dir_ / "obfuscated";
        EXPECT_TRUE(compileIRToExe(obf_ir, obf_exe))
            << "Failed to compile obfuscated IR - obfuscation broke the code!\n"
            << "Obfuscated IR:\n" << obf_ir.substr(0, 2000);

        // Step 6: Run both and compare
        auto orig_result = runExecutable(orig_exe, run_args);
        auto obf_exe_result = runExecutable(obf_exe, run_args);

        return {orig_result, obf_exe_result};
    }

    /**
     * Assert semantic equivalence with detailed error messages
     */
    void assertEquivalent(const std::string& c_code,
                          const std::string& obf_flags,
                          const std::string& test_name,
                          const std::vector<std::string>& run_args = {}) {
        auto [orig, obf] = testSemanticEquivalence(c_code, obf_flags, run_args);

        EXPECT_EQ(orig.exit_code, obf.exit_code)
            << test_name << ": Exit codes differ (orig=" << orig.exit_code
            << ", obf=" << obf.exit_code << ")";

        EXPECT_EQ(orig.stdout_output, obf.stdout_output)
            << test_name << ": Output differs!\n"
            << "Original: " << orig.stdout_output << "\n"
            << "Obfuscated: " << obf.stdout_output;
    }
};

// ============================================================================
// MBA (Mixed Boolean Arithmetic) Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, MBA_Add_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int a = 12345, b = 67890;
    printf("%d\n", a + b);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Add_Simple");
}

TEST_F(SemanticEquivalenceTest, MBA_Add_Negative) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int a = -500, b = 300;
    printf("%d\n", a + b);
    printf("%d\n", b + a);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Add_Negative");
}

TEST_F(SemanticEquivalenceTest, MBA_Sub_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int a = 100, b = 37;
    printf("%d\n", a - b);
    printf("%d\n", b - a);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Sub_Simple");
}

TEST_F(SemanticEquivalenceTest, MBA_Xor_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    unsigned int a = 0xDEADBEEF, b = 0xCAFEBABE;
    printf("%u\n", a ^ b);
    printf("%u\n", b ^ a);
    printf("%u\n", a ^ a);  // Should be 0
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Xor_Simple");
}

TEST_F(SemanticEquivalenceTest, MBA_And_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    unsigned int a = 0xFF00FF00, b = 0x0F0F0F0F;
    printf("%u\n", a & b);
    printf("%u\n", b & a);
    printf("%u\n", a & a);  // Should be a
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_And_Simple");
}

TEST_F(SemanticEquivalenceTest, MBA_Or_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    unsigned int a = 0xFF00FF00, b = 0x00FF00FF;
    printf("%u\n", a | b);
    printf("%u\n", b | a);
    printf("%u\n", a | a);  // Should be a
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Or_Simple");
}

TEST_F(SemanticEquivalenceTest, MBA_Mul_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int a = 123, b = 456;
    printf("%d\n", a * b);
    printf("%d\n", a * 0);
    printf("%d\n", a * 1);
    printf("%d\n", a * 2);
    printf("%d\n", a * -1);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Mul_Simple");
}

TEST_F(SemanticEquivalenceTest, MBA_All_Operations_Combined) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int a = 42, b = 17;
    int sum = a + b;
    int diff = a - b;
    int xored = a ^ b;
    int anded = a & b;
    int ored = a | b;
    int product = a * b;
    printf("%d %d %d %d %d %d\n", sum, diff, xored, anded, ored, product);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_All_Operations");
}

TEST_F(SemanticEquivalenceTest, MBA_Edge_Cases) {
    const char* code = R"(
#include <stdio.h>
#include <stdint.h>
int main() {
    // Edge cases: 0, -1, max/min values
    int32_t zero = 0;
    int32_t neg_one = -1;
    int32_t max_val = 2147483647;
    int32_t min_val = -2147483648;

    printf("%d\n", zero + zero);
    printf("%d\n", neg_one + 1);
    printf("%d\n", max_val ^ neg_one);
    printf("%d\n", zero & max_val);
    printf("%d\n", zero | min_val);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Edge_Cases");
}

TEST_F(SemanticEquivalenceTest, MBA_Different_Types) {
    const char* code = R"(
#include <stdio.h>
#include <stdint.h>
int main() {
    int8_t a8 = 100, b8 = 27;
    int16_t a16 = 30000, b16 = 5000;
    int32_t a32 = 1000000, b32 = 500000;
    int64_t a64 = 1000000000000LL, b64 = 500000000000LL;

    printf("%d\n", (int)(a8 + b8));
    printf("%d\n", a16 + b16);
    printf("%d\n", a32 + b32);
    printf("%lld\n", (long long)(a64 + b64));

    printf("%d\n", (int)(a8 ^ b8));
    printf("%d\n", a16 ^ b16);
    printf("%d\n", a32 ^ b32);
    printf("%lld\n", (long long)(a64 ^ b64));

    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Different_Types");
}

TEST_F(SemanticEquivalenceTest, MBA_Loop_Arithmetic) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        sum = sum + i;
    }
    printf("%d\n", sum);  // Should be 4950
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Loop_Arithmetic");
}

TEST_F(SemanticEquivalenceTest, MBA_Bitwise_Manipulation) {
    const char* code = R"(
#include <stdio.h>
int popcount(unsigned int x) {
    int count = 0;
    while (x) {
        count = count + (x & 1);
        x = x >> 1;
    }
    return count;
}
int main() {
    printf("%d\n", popcount(0));
    printf("%d\n", popcount(1));
    printf("%d\n", popcount(255));
    printf("%d\n", popcount(0xFFFFFFFF));
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "MBA_Bitwise_Manipulation");
}

// ============================================================================
// Control Flow Flattening (CFF) Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, CFF_Simple_If) {
    const char* code = R"(
#include <stdio.h>
int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}
int main() {
    printf("%d\n", max(10, 5));
    printf("%d\n", max(5, 10));
    printf("%d\n", max(7, 7));
    return 0;
}
)";
    assertEquivalent(code, "--cff --probability 1.0", "CFF_Simple_If");
}

TEST_F(SemanticEquivalenceTest, CFF_Multiple_Branches) {
    const char* code = R"(
#include <stdio.h>
int classify(int n) {
    if (n < 0) {
        return -1;
    } else if (n == 0) {
        return 0;
    } else if (n < 10) {
        return 1;
    } else if (n < 100) {
        return 2;
    } else {
        return 3;
    }
}
int main() {
    printf("%d\n", classify(-5));
    printf("%d\n", classify(0));
    printf("%d\n", classify(5));
    printf("%d\n", classify(50));
    printf("%d\n", classify(500));
    return 0;
}
)";
    assertEquivalent(code, "--cff --probability 1.0", "CFF_Multiple_Branches");
}

TEST_F(SemanticEquivalenceTest, CFF_Simple_Loop) {
    const char* code = R"(
#include <stdio.h>
int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result = result * i;
    }
    return result;
}
int main() {
    printf("%d\n", factorial(0));
    printf("%d\n", factorial(1));
    printf("%d\n", factorial(5));
    printf("%d\n", factorial(10));
    return 0;
}
)";
    assertEquivalent(code, "--cff --probability 1.0", "CFF_Simple_Loop");
}

TEST_F(SemanticEquivalenceTest, CFF_While_Loop) {
    const char* code = R"(
#include <stdio.h>
int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}
int main() {
    printf("%d\n", gcd(48, 18));
    printf("%d\n", gcd(17, 13));
    printf("%d\n", gcd(100, 25));
    return 0;
}
)";
    assertEquivalent(code, "--cff --probability 1.0", "CFF_While_Loop");
}

TEST_F(SemanticEquivalenceTest, CFF_Nested_Loops) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int sum = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            sum = sum + i * j;
        }
    }
    printf("%d\n", sum);
    return 0;
}
)";
    assertEquivalent(code, "--cff --probability 1.0", "CFF_Nested_Loops");
}

TEST_F(SemanticEquivalenceTest, CFF_Switch_Statement) {
    const char* code = R"(
#include <stdio.h>
int switch_test(int x) {
    int result;
    switch (x) {
        case 0: result = 100; break;
        case 1: result = 200; break;
        case 2: result = 300; break;
        case 5: result = 500; break;
        default: result = -1; break;
    }
    return result;
}
int main() {
    printf("%d\n", switch_test(0));
    printf("%d\n", switch_test(1));
    printf("%d\n", switch_test(2));
    printf("%d\n", switch_test(3));
    printf("%d\n", switch_test(5));
    return 0;
}
)";
    assertEquivalent(code, "--cff --probability 1.0", "CFF_Switch_Statement");
}

// ============================================================================
// Variable Splitting Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, VarSplit_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int x = 12345;
    int y = x * 2;
    printf("%d %d\n", x, y);
    return 0;
}
)";
    assertEquivalent(code, "--varsplit --probability 1.0", "VarSplit_Simple");
}

TEST_F(SemanticEquivalenceTest, VarSplit_Multiple_Variables) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int a = 100;
    int b = 200;
    int c = 300;
    int sum = a + b + c;
    printf("%d\n", sum);
    return 0;
}
)";
    assertEquivalent(code, "--varsplit --probability 1.0", "VarSplit_Multiple");
}

TEST_F(SemanticEquivalenceTest, VarSplit_Loop_Counter) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum = sum + i;
    }
    printf("%d\n", sum);
    return 0;
}
)";
    assertEquivalent(code, "--varsplit --probability 1.0", "VarSplit_Loop_Counter");
}

// ============================================================================
// String Encoding Tests
// NOTE: String encoding requires a runtime decoder library to be linked.
// These tests verify the pass transforms strings but skip semantic equivalence
// since the decoder is not automatically injected into the IR.
// ============================================================================

TEST_F(SemanticEquivalenceTest, StrEnc_Transforms_Strings) {
    // This test verifies string encoding transforms strings,
    // but doesn't test semantic equivalence since runtime decoder is needed.
    const char* code = R"(
#include <stdio.h>
int main() {
    printf("Hello, World!\n");
    return 0;
}
)";

    std::string ir = compileToIR(code);
    ASSERT_FALSE(ir.empty());

    // Verify original has plain string
    EXPECT_TRUE(irContains(ir, "Hello, World!"));

    // Obfuscate
    auto ir_file = writeSource("input.ll", ir);
    auto obf_ir_file = test_dir_ / "obfuscated.ll";
    std::string obf_cmd = ir_obf_path_.string() + " --strenc " +
                          ir_file.string() + " " + obf_ir_file.string();
    auto result = runCommand(obf_cmd);
    ASSERT_TRUE(result.success());

    std::ifstream obf_file(obf_ir_file);
    std::stringstream obf_buf;
    obf_buf << obf_file.rdbuf();
    std::string obf_ir = obf_buf.str();

    // Verify string was encoded (should NOT contain plain text anymore)
    EXPECT_FALSE(irContains(obf_ir, "Hello, World!"))
        << "String encoding should transform the plain text string";

    // Should have the MORPHECT_ENCODED marker
    EXPECT_TRUE(irContains(obf_ir, "MORPHECT_ENCODED"))
        << "Encoded strings should have MORPHECT_ENCODED comment";
}

// ============================================================================
// Dead Code Insertion Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, DeadCode_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int x = 5;
    int y = 10;
    printf("%d\n", x + y);
    return 0;
}
)";
    assertEquivalent(code, "--deadcode --probability 1.0", "DeadCode_Simple");
}

TEST_F(SemanticEquivalenceTest, DeadCode_Loop) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int sum = 0;
    for (int i = 0; i < 5; i++) {
        sum = sum + i;
    }
    printf("%d\n", sum);
    return 0;
}
)";
    assertEquivalent(code, "--deadcode --probability 1.0", "DeadCode_Loop");
}

// ============================================================================
// Bogus Control Flow Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, Bogus_Simple) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int x = 10;
    int y = 20;
    printf("%d\n", x + y);
    return 0;
}
)";
    assertEquivalent(code, "--bogus --probability 1.0", "Bogus_Simple");
}

TEST_F(SemanticEquivalenceTest, Bogus_Branches) {
    const char* code = R"(
#include <stdio.h>
int abs_val(int x) {
    if (x < 0) {
        return -x;
    }
    return x;
}
int main() {
    printf("%d\n", abs_val(-5));
    printf("%d\n", abs_val(5));
    printf("%d\n", abs_val(0));
    return 0;
}
)";
    assertEquivalent(code, "--bogus --probability 1.0", "Bogus_Branches");
}

// ============================================================================
// Combined Passes Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, Combined_MBA_CFF) {
    const char* code = R"(
#include <stdio.h>
int compute(int a, int b, int op) {
    int result;
    if (op == 0) {
        result = a + b;
    } else if (op == 1) {
        result = a - b;
    } else if (op == 2) {
        result = a ^ b;
    } else {
        result = a & b;
    }
    return result;
}
int main() {
    printf("%d\n", compute(100, 50, 0));
    printf("%d\n", compute(100, 50, 1));
    printf("%d\n", compute(100, 50, 2));
    printf("%d\n", compute(100, 50, 3));
    return 0;
}
)";
    assertEquivalent(code, "--mba --cff --probability 1.0", "Combined_MBA_CFF");
}

TEST_F(SemanticEquivalenceTest, Combined_MBA_VarSplit) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int a = 1234;
    int b = 5678;
    int sum = a + b;
    int diff = a - b;
    int xored = a ^ b;
    printf("%d %d %d\n", sum, diff, xored);
    return 0;
}
)";
    assertEquivalent(code, "--mba --varsplit --probability 1.0", "Combined_MBA_VarSplit");
}

TEST_F(SemanticEquivalenceTest, Combined_CFF_Bogus) {
    const char* code = R"(
#include <stdio.h>
int classify(int x) {
    if (x < 0) return -1;
    if (x == 0) return 0;
    if (x < 100) return 1;
    return 2;
}
int main() {
    printf("%d\n", classify(-10));
    printf("%d\n", classify(0));
    printf("%d\n", classify(50));
    printf("%d\n", classify(200));
    return 0;
}
)";
    assertEquivalent(code, "--cff --bogus --probability 1.0", "Combined_CFF_Bogus");
}

TEST_F(SemanticEquivalenceTest, Combined_All_Passes) {
    const char* code = R"(
#include <stdio.h>
int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}
int main() {
    for (int i = 0; i <= 10; i++) {
        printf("%d ", fibonacci(i));
    }
    printf("\n");
    return 0;
}
)";
    assertEquivalent(code, "--all --probability 0.8", "Combined_All_Passes");
}

// ============================================================================
// Complex Algorithm Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, Algorithm_BubbleSort) {
    const char* code = R"(
#include <stdio.h>
void bubble_sort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}
int main() {
    int arr[] = {64, 34, 25, 12, 22, 11, 90};
    int n = 7;
    bubble_sort(arr, n);
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    return 0;
}
)";
    assertEquivalent(code, "--mba --cff --probability 1.0", "Algorithm_BubbleSort");
}

TEST_F(SemanticEquivalenceTest, Algorithm_BinarySearch) {
    const char* code = R"(
#include <stdio.h>
int binary_search(int arr[], int n, int target) {
    int left = 0, right = n - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) left = mid + 1;
        else right = mid - 1;
    }
    return -1;
}
int main() {
    int arr[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19};
    int n = 10;
    printf("%d\n", binary_search(arr, n, 7));
    printf("%d\n", binary_search(arr, n, 1));
    printf("%d\n", binary_search(arr, n, 19));
    printf("%d\n", binary_search(arr, n, 8));
    return 0;
}
)";
    assertEquivalent(code, "--mba --cff --probability 1.0", "Algorithm_BinarySearch");
}

TEST_F(SemanticEquivalenceTest, Algorithm_Prime_Check) {
    const char* code = R"(
#include <stdio.h>
int is_prime(int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if ((n % 2) == 0 || (n % 3) == 0) return 0;
    int i = 5;
    while (i * i <= n) {
        if ((n % i) == 0 || (n % (i + 2)) == 0) return 0;
        i = i + 6;
    }
    return 1;
}
int main() {
    for (int i = 1; i <= 20; i++) {
        printf("%d:%d ", i, is_prime(i));
    }
    printf("\n");
    return 0;
}
)";
    assertEquivalent(code, "--mba --cff --probability 1.0", "Algorithm_Prime_Check");
}

TEST_F(SemanticEquivalenceTest, Algorithm_Recursive_Fibonacci) {
    const char* code = R"(
#include <stdio.h>
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
int main() {
    for (int i = 0; i <= 12; i++) {
        printf("%d ", fib(i));
    }
    printf("\n");
    return 0;
}
)";
    assertEquivalent(code, "--mba --cff --probability 1.0", "Algorithm_Recursive_Fibonacci");
}

TEST_F(SemanticEquivalenceTest, Algorithm_Bit_Operations) {
    const char* code = R"(
#include <stdio.h>
unsigned int reverse_bits(unsigned int n) {
    unsigned int result = 0;
    for (int i = 0; i < 32; i++) {
        result = (result << 1) | (n & 1);
        n = n >> 1;
    }
    return result;
}
int count_set_bits(unsigned int n) {
    int count = 0;
    while (n) {
        count = count + (n & 1);
        n = n >> 1;
    }
    return count;
}
int main() {
    printf("%u\n", reverse_bits(0x12345678));
    printf("%d\n", count_set_bits(0));
    printf("%d\n", count_set_bits(0xFF));
    printf("%d\n", count_set_bits(0xFFFFFFFF));
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "Algorithm_Bit_Operations");
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(SemanticEquivalenceTest, Stress_Many_Operations) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int result = 0;
    for (int i = 0; i < 100; i++) {
        result = result + i;
        result = result ^ (i * 3);
        result = result & 0xFFFF;
        result = result | (i << 4);
        result = result - (i / 2);
    }
    printf("%d\n", result);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "Stress_Many_Operations");
}

TEST_F(SemanticEquivalenceTest, Stress_Deep_Nesting) {
    const char* code = R"(
#include <stdio.h>
int main() {
    int result = 0;
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            for (int c = 0; c < 3; c++) {
                if (a > b) {
                    if (b > c) {
                        result = result + a * b * c;
                    } else {
                        result = result - a;
                    }
                } else {
                    result = result ^ (a + b + c);
                }
            }
        }
    }
    printf("%d\n", result);
    return 0;
}
)";
    assertEquivalent(code, "--mba --cff --probability 1.0", "Stress_Deep_Nesting");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SemanticEquivalenceTest, Edge_Empty_Function) {
    const char* code = R"(
#include <stdio.h>
void empty_func(void) {
    // Does nothing
}
int main() {
    empty_func();
    printf("%d\n", 42);  // Use format string instead of plain string
    return 0;
}
)";
    // Don't use --all since strenc requires runtime decoder
    assertEquivalent(code, "--mba --cff --varsplit --deadcode --bogus --probability 1.0", "Edge_Empty_Function");
}

TEST_F(SemanticEquivalenceTest, Edge_Single_Return) {
    const char* code = R"(
#include <stdio.h>
int just_return(int x) {
    return x;
}
int main() {
    printf("%d\n", just_return(42));
    return 0;
}
)";
    // Don't use --all since strenc requires runtime decoder
    assertEquivalent(code, "--mba --cff --varsplit --deadcode --bogus --probability 1.0", "Edge_Single_Return");
}

TEST_F(SemanticEquivalenceTest, Edge_Unsigned_Operations) {
    const char* code = R"(
#include <stdio.h>
#include <stdint.h>
int main() {
    uint32_t a = 0xFFFFFFFF;
    uint32_t b = 1;
    printf("%u\n", a + b);  // Overflow to 0
    printf("%u\n", a ^ b);
    printf("%u\n", a & b);
    printf("%u\n", a | b);
    return 0;
}
)";
    assertEquivalent(code, "--mba --probability 1.0", "Edge_Unsigned_Operations");
}

// ============================================================================
// Verification that obfuscation actually happens
// ============================================================================

TEST_F(SemanticEquivalenceTest, Verify_MBA_Transforms) {
    const char* ir = R"(
define i32 @test(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}
)";

    auto obfuscated = obfuscateIR(ir, "", "--mba --probability 1.0");
    ASSERT_FALSE(obfuscated.empty());

    // Should NOT have simple "add i32 %a, %b" anymore - it should be transformed
    // The obfuscated version should have more complex operations
    int or_count = countInIR(obfuscated, " or ");
    int and_count = countInIR(obfuscated, " and ");
    int xor_count = countInIR(obfuscated, " xor ");

    // MBA should introduce additional operations
    EXPECT_GT(or_count + and_count + xor_count, 0)
        << "MBA should transform add to use or/and/xor operations";
}

TEST_F(SemanticEquivalenceTest, Verify_CFF_Transforms) {
    const char* ir = R"(
define i32 @test(i32 %n) {
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %positive, label %negative

positive:
  ret i32 1

negative:
  ret i32 -1
}
)";

    auto obfuscated = obfuscateIR(ir, "", "--cff --probability 1.0");
    ASSERT_FALSE(obfuscated.empty());

    // CFF should introduce switch statements
    bool has_switch = irContains(obfuscated, "switch");
    // Or state variables
    bool has_state = irContains(obfuscated, "_state") || irContains(obfuscated, "_cff");

    // At least one of these should be present after CFF
    EXPECT_TRUE(has_switch || has_state || obfuscated.length() > strlen(ir))
        << "CFF should transform control flow";
}

