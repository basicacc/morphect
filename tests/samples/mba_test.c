/**
 * MBA Obfuscation Test Suite
 *
 * This file tests all MBA operations to verify correctness
 * after obfuscation.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Test cases for ADD
int test_add(int a, int b) {
    return a + b;
}

// Test cases for SUB
int test_sub(int a, int b) {
    return a - b;
}

// Test cases for XOR
int test_xor(int a, int b) {
    return a ^ b;
}

// Test cases for AND
int test_and(int a, int b) {
    return a & b;
}

// Test cases for OR
int test_or(int a, int b) {
    return a | b;
}

// Test cases for MULT
int test_mult_2(int a) {
    return a * 2;
}

int test_mult_3(int a) {
    return a * 3;
}

int test_mult_5(int a) {
    return a * 5;
}

int test_mult_7(int a) {
    return a * 7;
}

int test_mult_8(int a) {
    return a * 8;
}

int test_mult_9(int a) {
    return a * 9;
}

int test_mult_15(int a) {
    return a * 15;
}

int test_mult_17(int a) {
    return a * 17;
}

// Complex expressions combining multiple operations
int test_complex1(int a, int b, int c) {
    // (a + b) ^ c
    return (a + b) ^ c;
}

int test_complex2(int a, int b, int c) {
    // (a | b) & (c ^ a)
    return (a | b) & (c ^ a);
}

int test_complex3(int a, int b, int c) {
    // ((a + b) - c) * 3
    return ((a + b) - c) * 3;
}

int test_complex4(int a, int b) {
    // (a ^ b) + (a & b) should equal (a | b)
    return (a ^ b) + (a & b);
}

int test_complex5(int a, int b) {
    // (a | b) - (a & b) should equal (a ^ b)
    return (a | b) - (a & b);
}

// Test with unsigned types
unsigned int test_unsigned_add(unsigned int a, unsigned int b) {
    return a + b;
}

unsigned int test_unsigned_xor(unsigned int a, unsigned int b) {
    return a ^ b;
}

// Test with different bit widths
int64_t test_64bit_add(int64_t a, int64_t b) {
    return a + b;
}

int64_t test_64bit_xor(int64_t a, int64_t b) {
    return a ^ b;
}

int16_t test_16bit_add(int16_t a, int16_t b) {
    return a + b;
}

int8_t test_8bit_xor(int8_t a, int8_t b) {
    return a ^ b;
}

// Edge case tests
int test_edge_zero(int a) {
    return a + 0;
}

int test_edge_negone(int a) {
    return a ^ (-1);  // Should be ~a
}

int test_edge_self_xor(int a) {
    return a ^ a;  // Should always be 0
}

int test_edge_self_and(int a) {
    return a & a;  // Should always be a
}

int test_edge_self_or(int a) {
    return a | a;  // Should always be a
}

// Loop with arithmetic - tests that obfuscation preserves loop behavior
int test_loop_sum(int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum = sum + i;
    }
    return sum;
}

// Nested operations for deeper MBA nesting
int test_nested(int a, int b, int c, int d) {
    int t1 = a + b;
    int t2 = c ^ d;
    int t3 = t1 & t2;
    int t4 = t1 | t2;
    return t3 + t4;
}

// Test values
typedef struct {
    int a;
    int b;
    int expected_add;
    int expected_sub;
    int expected_xor;
    int expected_and;
    int expected_or;
} TestCase;

static TestCase test_cases[] = {
    // Basic cases
    {0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 1, 1, 0, 1},
    {0, 1, 1, -1, 1, 0, 1},
    {1, 1, 2, 0, 0, 1, 1},

    // Positive numbers
    {5, 3, 8, 2, 6, 1, 7},
    {10, 7, 17, 3, 13, 2, 15},
    {100, 50, 150, 50, 86, 32, 118},
    {255, 128, 383, 127, 127, 128, 255},

    // Negative numbers
    {-1, -1, -2, 0, 0, -1, -1},
    {-5, 3, -2, -8, -8, 3, -5},
    {5, -3, 2, 8, -8, 5, -3},
    {-10, -7, -17, -3, 15, -16, -1},

    // Powers of 2
    {2, 4, 6, -2, 6, 0, 6},
    {8, 16, 24, -8, 24, 0, 24},
    {32, 64, 96, -32, 96, 0, 96},
    {128, 256, 384, -128, 384, 0, 384},

    // Edge cases
    {2147483647, 0, 2147483647, 2147483647, 2147483647, 0, 2147483647},
    {-2147483648, 0, -2147483648, -2147483648, -2147483648, 0, -2147483648},
    {0x55555555, 0xAAAAAAAA, -1, -1431655765, -1, 0, -1},  // Alternating bits
    {0x0F0F0F0F, 0xF0F0F0F0, -1, 505290271, -1, 0, -1},   // Alternating nibbles
};

int failures = 0;

#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { \
        printf("FAIL: " fmt "\n", ##__VA_ARGS__); \
        failures++; \
    } \
} while(0)

void run_basic_tests() {
    printf("Running basic operation tests...\n");

    int n = sizeof(test_cases) / sizeof(test_cases[0]);
    for (int i = 0; i < n; i++) {
        TestCase tc = test_cases[i];

        CHECK(test_add(tc.a, tc.b) == tc.expected_add,
              "add(%d, %d) = %d, expected %d",
              tc.a, tc.b, test_add(tc.a, tc.b), tc.expected_add);

        CHECK(test_sub(tc.a, tc.b) == tc.expected_sub,
              "sub(%d, %d) = %d, expected %d",
              tc.a, tc.b, test_sub(tc.a, tc.b), tc.expected_sub);

        CHECK(test_xor(tc.a, tc.b) == tc.expected_xor,
              "xor(%d, %d) = %d, expected %d",
              tc.a, tc.b, test_xor(tc.a, tc.b), tc.expected_xor);

        CHECK(test_and(tc.a, tc.b) == tc.expected_and,
              "and(%d, %d) = %d, expected %d",
              tc.a, tc.b, test_and(tc.a, tc.b), tc.expected_and);

        CHECK(test_or(tc.a, tc.b) == tc.expected_or,
              "or(%d, %d) = %d, expected %d",
              tc.a, tc.b, test_or(tc.a, tc.b), tc.expected_or);
    }
}

void run_mult_tests() {
    printf("Running multiplication tests...\n");

    int test_vals[] = {0, 1, -1, 5, -5, 10, -10, 100, -100, 1000};
    int n = sizeof(test_vals) / sizeof(test_vals[0]);

    for (int i = 0; i < n; i++) {
        int a = test_vals[i];

        CHECK(test_mult_2(a) == a * 2, "mult_2(%d) = %d, expected %d", a, test_mult_2(a), a * 2);
        CHECK(test_mult_3(a) == a * 3, "mult_3(%d) = %d, expected %d", a, test_mult_3(a), a * 3);
        CHECK(test_mult_5(a) == a * 5, "mult_5(%d) = %d, expected %d", a, test_mult_5(a), a * 5);
        CHECK(test_mult_7(a) == a * 7, "mult_7(%d) = %d, expected %d", a, test_mult_7(a), a * 7);
        CHECK(test_mult_8(a) == a * 8, "mult_8(%d) = %d, expected %d", a, test_mult_8(a), a * 8);
        CHECK(test_mult_9(a) == a * 9, "mult_9(%d) = %d, expected %d", a, test_mult_9(a), a * 9);
        CHECK(test_mult_15(a) == a * 15, "mult_15(%d) = %d, expected %d", a, test_mult_15(a), a * 15);
        CHECK(test_mult_17(a) == a * 17, "mult_17(%d) = %d, expected %d", a, test_mult_17(a), a * 17);
    }
}

void run_complex_tests() {
    printf("Running complex expression tests...\n");

    int vals[] = {0, 1, -1, 5, -5, 10, 42, 100, 255, -128};
    int n = sizeof(vals) / sizeof(vals[0]);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                int a = vals[i], b = vals[j], c = vals[k];

                CHECK(test_complex1(a, b, c) == ((a + b) ^ c),
                      "complex1(%d, %d, %d) = %d, expected %d",
                      a, b, c, test_complex1(a, b, c), (a + b) ^ c);

                CHECK(test_complex2(a, b, c) == ((a | b) & (c ^ a)),
                      "complex2(%d, %d, %d) = %d, expected %d",
                      a, b, c, test_complex2(a, b, c), (a | b) & (c ^ a));

                CHECK(test_complex3(a, b, c) == (((a + b) - c) * 3),
                      "complex3(%d, %d, %d) = %d, expected %d",
                      a, b, c, test_complex3(a, b, c), ((a + b) - c) * 3);
            }
        }
    }

    // Test MBA identities
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            int a = vals[i], b = vals[j];

            // (a ^ b) + (a & b) == (a | b)
            CHECK(test_complex4(a, b) == (a | b),
                  "identity (a^b)+(a&b)==(a|b) failed for a=%d, b=%d: got %d, expected %d",
                  a, b, test_complex4(a, b), a | b);

            // (a | b) - (a & b) == (a ^ b)
            CHECK(test_complex5(a, b) == (a ^ b),
                  "identity (a|b)-(a&b)==(a^b) failed for a=%d, b=%d: got %d, expected %d",
                  a, b, test_complex5(a, b), a ^ b);
        }
    }
}

void run_edge_tests() {
    printf("Running edge case tests...\n");

    int vals[] = {0, 1, -1, 127, -128, 2147483647, -2147483648};
    int n = sizeof(vals) / sizeof(vals[0]);

    for (int i = 0; i < n; i++) {
        int a = vals[i];

        CHECK(test_edge_zero(a) == a, "edge_zero(%d) = %d, expected %d", a, test_edge_zero(a), a);
        CHECK(test_edge_negone(a) == ~a, "edge_negone(%d) = %d, expected %d", a, test_edge_negone(a), ~a);
        CHECK(test_edge_self_xor(a) == 0, "edge_self_xor(%d) = %d, expected 0", a, test_edge_self_xor(a));
        CHECK(test_edge_self_and(a) == a, "edge_self_and(%d) = %d, expected %d", a, test_edge_self_and(a), a);
        CHECK(test_edge_self_or(a) == a, "edge_self_or(%d) = %d, expected %d", a, test_edge_self_or(a), a);
    }
}

void run_loop_tests() {
    printf("Running loop tests...\n");

    for (int n = 0; n <= 100; n++) {
        int expected = (n * (n - 1)) / 2;  // Sum of 0..n-1
        CHECK(test_loop_sum(n) == expected, "loop_sum(%d) = %d, expected %d", n, test_loop_sum(n), expected);
    }
}

void run_nested_tests() {
    printf("Running nested operation tests...\n");

    int vals[] = {0, 1, -1, 5, 42, 255};
    int n = sizeof(vals) / sizeof(vals[0]);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                for (int l = 0; l < n; l++) {
                    int a = vals[i], b = vals[j], c = vals[k], d = vals[l];

                    int t1 = a + b;
                    int t2 = c ^ d;
                    int t3 = t1 & t2;
                    int t4 = t1 | t2;
                    int expected = t3 + t4;

                    CHECK(test_nested(a, b, c, d) == expected,
                          "nested(%d, %d, %d, %d) = %d, expected %d",
                          a, b, c, d, test_nested(a, b, c, d), expected);
                }
            }
        }
    }
}

void run_bitwidth_tests() {
    printf("Running different bit-width tests...\n");

    // 64-bit tests
    int64_t v64[] = {0, 1, -1, 0x7FFFFFFFFFFFFFFFLL, (int64_t)0x8000000000000000LL};
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            int64_t a = v64[i], b = v64[j];
            CHECK(test_64bit_add(a, b) == a + b, "64bit_add failed");
            CHECK(test_64bit_xor(a, b) == (a ^ b), "64bit_xor failed");
        }
    }

    // 16-bit tests
    int16_t v16[] = {0, 1, -1, 32767, -32768};
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            int16_t a = v16[i], b = v16[j];
            CHECK(test_16bit_add(a, b) == (int16_t)(a + b), "16bit_add failed");
        }
    }

    // 8-bit tests
    int8_t v8[] = {0, 1, -1, 127, -128};
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            int8_t a = v8[i], b = v8[j];
            CHECK(test_8bit_xor(a, b) == (int8_t)(a ^ b), "8bit_xor failed");
        }
    }

    // Unsigned tests
    unsigned int uvals[] = {0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF};
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            unsigned int a = uvals[i], b = uvals[j];
            CHECK(test_unsigned_add(a, b) == a + b, "unsigned_add failed");
            CHECK(test_unsigned_xor(a, b) == (a ^ b), "unsigned_xor failed");
        }
    }
}

int main() {
    printf("=== MBA Obfuscation Correctness Test Suite ===\n\n");

    run_basic_tests();
    run_mult_tests();
    run_complex_tests();
    run_edge_tests();
    run_loop_tests();
    run_nested_tests();
    run_bitwidth_tests();

    printf("\n=== Test Results ===\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED!\n");
        return 0;
    } else {
        printf("FAILED: %d test(s) failed\n", failures);
        return 1;
    }
}
