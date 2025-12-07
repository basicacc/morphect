/**
 * Morphect - GIMPLE Plugin Integration Tests
 *
 * Tests that the GIMPLE plugin produces correct output
 * for various C programs.
 */

#include <gtest/gtest.h>
#include "../fixtures/obfuscation_fixture.hpp"

using namespace morphect::test;

class GimpleIntegrationTest : public ObfuscationFixture {
protected:
    void SetUp() override {
        ObfuscationFixture::SetUp();

        // Skip tests if plugin doesn't exist
        if (!std::filesystem::exists(plugin_path_)) {
            GTEST_SKIP() << "GIMPLE plugin not found at: " << plugin_path_;
        }
    }
};

// ============================================================================
// Basic Arithmetic Tests
// ============================================================================

TEST_F(GimpleIntegrationTest, SimpleAddition) {
    const char* source = R"(
#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main() {
    printf("%d\n", add(10, 20));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, SimpleSubtraction) {
    const char* source = R"(
#include <stdio.h>

int sub(int a, int b) {
    return a - b;
}

int main() {
    printf("%d\n", sub(30, 12));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, BitwiseOperations) {
    const char* source = R"(
#include <stdio.h>

int bitwise(int a, int b) {
    int x = a ^ b;
    int y = a & b;
    int z = a | b;
    return x + y + z;
}

int main() {
    printf("%d\n", bitwise(0x55, 0xAA));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, ComplexArithmetic) {
    const char* source = R"(
#include <stdio.h>

int compute(int a, int b, int c) {
    int t1 = a + b;
    int t2 = t1 - c;
    int t3 = t2 ^ a;
    int t4 = t3 & b;
    int t5 = t4 | c;
    return t5 + (a * 2) - (b / 2);
}

int main() {
    printf("%d\n", compute(100, 50, 25));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

// ============================================================================
// Loop Tests
// ============================================================================

TEST_F(GimpleIntegrationTest, SimpleLoop) {
    const char* source = R"(
#include <stdio.h>

int sum(int n) {
    int result = 0;
    for (int i = 1; i <= n; i++) {
        result += i;
    }
    return result;
}

int main() {
    printf("%d\n", sum(100));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, NestedLoops) {
    const char* source = R"(
#include <stdio.h>

int nested(int n) {
    int result = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            result += i + j;
        }
    }
    return result;
}

int main() {
    printf("%d\n", nested(10));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

// ============================================================================
// Conditional Tests
// ============================================================================

TEST_F(GimpleIntegrationTest, SimpleConditional) {
    const char* source = R"(
#include <stdio.h>

int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

int main() {
    printf("%d\n", max(42, 17));
    printf("%d\n", max(17, 42));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, SwitchStatement) {
    const char* source = R"(
#include <stdio.h>

int classify(int n) {
    switch (n % 4) {
        case 0: return 10;
        case 1: return 20;
        case 2: return 30;
        default: return 40;
    }
}

int main() {
    for (int i = 0; i < 8; i++) {
        printf("%d ", classify(i));
    }
    printf("\n");
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

// ============================================================================
// Pointer and Array Tests
// ============================================================================

TEST_F(GimpleIntegrationTest, ArraySum) {
    const char* source = R"(
#include <stdio.h>

int array_sum(int* arr, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

int main() {
    int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printf("%d\n", array_sum(arr, 10));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, PointerArithmetic) {
    const char* source = R"(
#include <stdio.h>

int pointer_sum(int* start, int* end) {
    int sum = 0;
    while (start < end) {
        sum += *start;
        start++;
    }
    return sum;
}

int main() {
    int arr[] = {5, 10, 15, 20, 25};
    printf("%d\n", pointer_sum(arr, arr + 5));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

// ============================================================================
// Recursion Tests
// ============================================================================

TEST_F(GimpleIntegrationTest, Factorial) {
    const char* source = R"(
#include <stdio.h>

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main() {
    printf("%d\n", factorial(10));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, Fibonacci) {
    const char* source = R"(
#include <stdio.h>

int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    for (int i = 0; i < 15; i++) {
        printf("%d ", fib(i));
    }
    printf("\n");
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(GimpleIntegrationTest, NegativeNumbers) {
    const char* source = R"(
#include <stdio.h>

int compute(int a, int b) {
    return (a + b) ^ (a - b) & (a | b);
}

int main() {
    printf("%d\n", compute(-100, 50));
    printf("%d\n", compute(100, -50));
    printf("%d\n", compute(-100, -50));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, ZeroOperations) {
    const char* source = R"(
#include <stdio.h>

int with_zero(int a) {
    int x = a + 0;
    int y = a - 0;
    int z = a ^ 0;
    int w = a & 0;
    int v = a | 0;
    return x + y + z + w + v;
}

int main() {
    printf("%d\n", with_zero(42));
    printf("%d\n", with_zero(0));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}

TEST_F(GimpleIntegrationTest, LargeNumbers) {
    const char* source = R"(
#include <stdio.h>

long long big_compute(long long a, long long b) {
    return (a + b) ^ (a & b);
}

int main() {
    printf("%lld\n", big_compute(1000000000LL, 2000000000LL));
    return 0;
}
)";
    assertSemanticEquivalence(source);
}
