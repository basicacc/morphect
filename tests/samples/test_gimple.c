/*
 * Sample C code for testing GIMPLE backend (GCC Plugin)
 *
 * Build and test:
 *   gcc -fplugin=./build/lib/morphect_plugin.so \
 *       -fplugin-arg-morphect_plugin-config=config/morphect_config.json \
 *       test_gimple.c -o test_gimple_obf
 *   ./test_gimple_obf
 *
 * Or without config (uses defaults):
 *   gcc -fplugin=./build/lib/morphect_plugin.so test_gimple.c -o test_gimple_obf
 *
 * To see verbose output:
 *   gcc -fplugin=./build/lib/morphect_plugin.so \
 *       -fplugin-arg-morphect_plugin-verbose \
 *       test_gimple.c -o test_gimple_obf
 */

#include <stdio.h>
#include <string.h>

// ==================================================
// MBA (Mixed Boolean Arithmetic) Test Functions
// These simple operations will be transformed into
// complex bitwise equivalents at the GIMPLE level
// ==================================================

int gimple_add(int x, int y) {
    // At GIMPLE: will become (x ^ y) + 2 * (x & y)
    return x + y;
}

int gimple_sub(int x, int y) {
    // At GIMPLE: complex subtraction transformation
    return x - y;
}

int gimple_mult(int x, int y) {
    // Multiplication may use shift-based MBA
    return x * y;
}

int combined_ops(int a, int b, int c) {
    // Multiple operations = multiple MBA transformations
    int sum = a + b;
    int diff = sum - c;
    int product = diff * 2;
    return product ^ a;
}

// ==================================================
// Control Flow Tests
// CFF, bogus control flow, opaque predicates
// ==================================================

// Simple branching - target for bogus control flow
int simple_branch(int val) {
    if (val > 0) {
        return val * 2;
    } else {
        return val * -1;
    }
}

// Multiple conditions - good CFF target
int multi_condition(int a, int b) {
    int result = 0;

    if (a > 10) {
        result += 100;
    }
    if (b > 20) {
        result += 200;
    }
    if (a + b > 25) {
        result += 300;
    }

    return result;
}

// Nested control flow - complex CFF
int nested_flow(int x, int y) {
    int r = 0;

    if (x > 0) {
        if (y > 0) {
            r = x + y;
        } else {
            r = x - y;
        }
    } else {
        if (y > 0) {
            r = y - x;
        } else {
            r = -(x + y);
        }
    }

    return r;
}

// Loop transformation target
int loop_sum(int n) {
    int sum = 0;
    for (int i = 1; i <= n; i++) {
        sum += i;
        if (i % 2 == 0) {
            sum += 1;  // Extra for even numbers
        }
    }
    return sum;
}

// ==================================================
// Data Obfuscation Tests
// String encoding, constant obfuscation
// ==================================================

// String that should be encoded
void hidden_print() {
    // This string should be XOR encoded in the binary
    printf("Secret: The password is morphect123\n");
}

// Constants that should be obfuscated
int use_constants() {
    int magic1 = 0x12345678;
    int magic2 = 0xCAFEBABE;
    int magic3 = 42;

    return (magic1 ^ magic2) + magic3;
}

// Variable splitting candidate
int var_split_test(int input) {
    int sensitive = input * 0x1337;
    sensitive += 0xDEAD;
    sensitive ^= 0xBEEF;
    return sensitive;
}

// ==================================================
// Dead Code Insertion Test
// These functions are good targets for dead code
// ==================================================

int dead_code_target(int a, int b) {
    // Simple function where dead code can be inserted
    int x = a + b;
    int y = x * 2;
    int z = y - a;
    return z;
}

// ==================================================
// Anti-Debug Test (if enabled)
// ==================================================

int sensitive_calculation(int key) {
    // This function could have anti-debug checks inserted
    int result = key;
    result ^= 0x5A5A5A5A;
    result = (result << 3) | (result >> 29);
    result += 0x13371337;
    return result;
}

// ==================================================
// Main - Verifies correctness after obfuscation
// ==================================================

int main() {
    printf("=== GIMPLE Backend Test (GCC Plugin) ===\n\n");

    // MBA tests - verify mathematical correctness
    printf("--- MBA Transformation Tests ---\n");
    printf("gimple_add(100, 50) = %d (expect 150)\n", gimple_add(100, 50));
    printf("gimple_sub(100, 50) = %d (expect 50)\n", gimple_sub(100, 50));
    printf("gimple_mult(7, 8) = %d (expect 56)\n", gimple_mult(7, 8));
    printf("combined_ops(5, 10, 3) = %d\n", combined_ops(5, 10, 3));

    // Control flow tests
    printf("\n--- Control Flow Tests ---\n");
    printf("simple_branch(10) = %d (expect 20)\n", simple_branch(10));
    printf("simple_branch(-5) = %d (expect 5)\n", simple_branch(-5));
    printf("multi_condition(15, 25) = %d (expect 600)\n", multi_condition(15, 25));
    printf("nested_flow(5, 3) = %d (expect 8)\n", nested_flow(5, 3));
    printf("nested_flow(-5, 3) = %d (expect 8)\n", nested_flow(-5, 3));
    printf("nested_flow(-5, -3) = %d (expect 8)\n", nested_flow(-5, -3));
    printf("loop_sum(5) = %d (expect 17)\n", loop_sum(5));

    // Data obfuscation tests
    printf("\n--- Data Obfuscation Tests ---\n");
    hidden_print();
    printf("use_constants() = 0x%X\n", use_constants());
    printf("var_split_test(100) = %d\n", var_split_test(100));

    // Dead code target
    printf("\n--- Dead Code Tests ---\n");
    printf("dead_code_target(10, 20) = %d (expect 40)\n", dead_code_target(10, 20));

    // Sensitive calculation
    printf("\n--- Sensitive Function Tests ---\n");
    printf("sensitive_calculation(0x1234) = 0x%X\n", sensitive_calculation(0x1234));

    printf("\n=== All Tests Complete ===\n");
    printf("If you see this and all values match expectations,\n");
    printf("the obfuscation preserved functionality correctly!\n");

    return 0;
}
