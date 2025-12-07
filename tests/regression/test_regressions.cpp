/**
 * Morphect - Regression Tests
 *
 * Tests for previously fixed bugs and known edge cases.
 * Each test documents the issue it prevents.
 */

#include <gtest/gtest.h>
#include "../fixtures/obfuscation_fixture.hpp"

using namespace morphect::test;

class RegressionTest : public ObfuscationFixture {
};

// ============================================================================
// Issue #001: SSA form corruption with create_tmp_var
// Fixed: Changed to make_ssa_name for proper SSA variable creation
// ============================================================================

TEST_F(RegressionTest, SSAFormIntegrity) {
    // This test ensures SSA form is maintained after transformation
    const char* source = R"(
#include <stdio.h>

int compute(int a, int b) {
    int x = a + b;      // SSA: x_1
    int y = x + a;      // SSA: y_1, uses x_1
    int z = y + b;      // SSA: z_1, uses y_1
    return z;
}

int main() {
    printf("%d\n", compute(5, 10));
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #002: Negative number handling in XOR operations
// XOR with negative numbers must work correctly with two's complement
// ============================================================================

TEST_F(RegressionTest, NegativeXOR) {
    const char* source = R"(
#include <stdio.h>

int test(int a, int b) {
    return a ^ b;
}

int main() {
    printf("%d\n", test(-1, 5));         // -1 = all 1s in binary
    printf("%d\n", test(-100, -200));
    printf("%d\n", test(0x80000000, 1)); // Min int
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #003: AND with -1 should return the other operand
// a & -1 = a (since -1 is all 1s)
// ============================================================================

TEST_F(RegressionTest, AndWithAllOnes) {
    const char* source = R"(
#include <stdio.h>

int test(int a) {
    return a & (-1);
}

int main() {
    printf("%d\n", test(42));
    printf("%d\n", test(-42));
    printf("%d\n", test(0));
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #004: OR with 0 should return the other operand
// a | 0 = a
// ============================================================================

TEST_F(RegressionTest, OrWithZero) {
    const char* source = R"(
#include <stdio.h>

int test(int a) {
    return a | 0;
}

int main() {
    printf("%d\n", test(42));
    printf("%d\n", test(-42));
    printf("%d\n", test(0));
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #005: Subtraction with itself should be 0
// a - a = 0
// ============================================================================

TEST_F(RegressionTest, SubtractionWithItself) {
    const char* source = R"(
#include <stdio.h>

int test(int a) {
    return a - a;
}

int main() {
    printf("%d\n", test(42));
    printf("%d\n", test(-42));
    printf("%d\n", test(0));
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #006: Overflow behavior must be preserved
// Signed overflow is undefined, but unsigned must wrap
// ============================================================================

TEST_F(RegressionTest, UnsignedOverflow) {
    const char* source = R"(
#include <stdio.h>
#include <limits.h>

unsigned int test(unsigned int a, unsigned int b) {
    return a + b;
}

int main() {
    printf("%u\n", test(UINT_MAX, 1));  // Should wrap to 0
    printf("%u\n", test(UINT_MAX, UINT_MAX));
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #007: Pointer arithmetic must work correctly
// Transformations should not affect pointer operations
// ============================================================================

TEST_F(RegressionTest, PointerArithmeticPreserved) {
    const char* source = R"(
#include <stdio.h>

int sum_array(int* arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += *(arr + i);  // Pointer arithmetic
    }
    return sum;
}

int main() {
    int arr[] = {1, 2, 3, 4, 5};
    printf("%d\n", sum_array(arr, 5));
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #008: Function calls with computed arguments
// Arguments computed with obfuscated arithmetic must still work
// ============================================================================

TEST_F(RegressionTest, ComputedFunctionArguments) {
    const char* source = R"(
#include <stdio.h>

void print_val(int val) {
    printf("%d\n", val);
}

int main() {
    int a = 10;
    int b = 20;
    print_val(a + b);
    print_val(a ^ b);
    print_val(a & b);
    print_val(a | b);
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #009: Return value must be computed correctly
// Final return value after obfuscation must be correct
// ============================================================================

TEST_F(RegressionTest, ReturnValueCorrectness) {
    const char* source = R"(
int compute(int n) {
    int a = n + 5;
    int b = a ^ 3;
    int c = b & 0xFF;
    int d = c | 0x100;
    return d - n;
}

int main() {
    // Use exit code as return value
    return compute(10);
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    auto source_path = writeSource("test.c", source);
    auto normal_exe = test_dir_ / "normal";
    auto obf_exe = test_dir_ / "obf";

    auto r1 = compileNormal(source_path, normal_exe);
    ASSERT_TRUE(r1.success());

    auto r2 = compileWithGimple(source_path, obf_exe);
    ASSERT_TRUE(r2.success());

    auto normal_result = runExecutable(normal_exe);
    auto obf_result = runExecutable(obf_exe);

    EXPECT_EQ(normal_result.exit_code, obf_result.exit_code);
}

// ============================================================================
// Issue #010: Multiple operations in single expression
// Complex expressions with multiple operators must work
// ============================================================================

TEST_F(RegressionTest, ComplexExpression) {
    const char* source = R"(
#include <stdio.h>

int complex(int a, int b, int c, int d) {
    return ((a + b) ^ (c & d)) | ((a - c) & (b ^ d));
}

int main() {
    printf("%d\n", complex(0x55, 0xAA, 0x0F, 0xF0));
    printf("%d\n", complex(100, 200, 300, 400));
    printf("%d\n", complex(-1, -2, -3, -4));
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #011: Structure member access after computation
// Struct members computed with obfuscated ops must be correct
// ============================================================================

TEST_F(RegressionTest, StructMemberComputation) {
    const char* source = R"(
#include <stdio.h>

struct Point {
    int x;
    int y;
};

struct Point add_points(struct Point a, struct Point b) {
    struct Point result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

int main() {
    struct Point p1 = {10, 20};
    struct Point p2 = {30, 40};
    struct Point p3 = add_points(p1, p2);
    printf("%d %d\n", p3.x, p3.y);
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}

// ============================================================================
// Issue #012: Global variable operations
// Operations on global variables must work correctly
// ============================================================================

TEST_F(RegressionTest, GlobalVariableOperations) {
    const char* source = R"(
#include <stdio.h>

int global_a = 100;
int global_b = 200;

int compute_global() {
    return global_a + global_b;
}

void modify_global(int delta) {
    global_a = global_a + delta;
    global_b = global_b ^ delta;
}

int main() {
    printf("%d\n", compute_global());
    modify_global(50);
    printf("%d\n", compute_global());
    return 0;
}
)";

    if (!std::filesystem::exists(plugin_path_)) {
        GTEST_SKIP() << "GIMPLE plugin not found";
    }

    assertSemanticEquivalence(source);
}
