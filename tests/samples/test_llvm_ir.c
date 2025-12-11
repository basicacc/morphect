/*
 * Sample C code for testing LLVM IR backend (morphect-ir)
 *
 * Build and test:
 *   clang -S -emit-llvm -O0 test_llvm_ir.c -o test_llvm_ir.ll
 *   ./build/bin/morphect-ir test_llvm_ir.ll obfuscated_ir.ll
 *   llc obfuscated_ir.ll -o obfuscated_ir.s
 *   gcc obfuscated_ir.s -o test_ir_obf
 *   ./test_ir_obf
 *
 * Or with config:
 *   ./build/bin/morphect-ir --config config/morphect_config.json test_llvm_ir.ll obfuscated_ir.ll
 */

#include <stdio.h>
#include <stdlib.h>

// MBA transformation targets
int mba_add(int a, int b) {
    return a + b;  // -> (a ^ b) + 2 * (a & b)
}

int mba_sub(int a, int b) {
    return a - b;  // -> (a ^ b) - 2 * (~a & b)
}

int mba_xor(int a, int b) {
    return a ^ b;  // -> (a | b) - (a & b)
}

int mba_and(int a, int b) {
    return a & b;  // -> (a | b) - (a ^ b)
}

int mba_or(int a, int b) {
    return a | b;  // -> (a ^ b) + (a & b)
}

// Chained MBA - multiple operations in one function
int complex_math(int x, int y, int z) {
    int t1 = x + y;    // MBA target
    int t2 = t1 ^ z;   // MBA target
    int t3 = t2 & x;   // MBA target
    return t3 | y;     // MBA target
}

// Control flow flattening targets
int calculate_grade(int score) {
    if (score >= 90) {
        return 4;  // A
    } else if (score >= 80) {
        return 3;  // B
    } else if (score >= 70) {
        return 2;  // C
    } else if (score >= 60) {
        return 1;  // D
    } else {
        return 0;  // F
    }
}

// Switch statement - CFF target
const char* day_name(int day) {
    switch (day) {
        case 1: return "Monday";
        case 2: return "Tuesday";
        case 3: return "Wednesday";
        case 4: return "Thursday";
        case 5: return "Friday";
        case 6: return "Saturday";
        case 7: return "Sunday";
        default: return "Invalid";
    }
}

// Loop with conditional - bogus CF target
int fibonacci(int n) {
    if (n <= 1) return n;

    int a = 0, b = 1, temp;
    for (int i = 2; i <= n; i++) {
        temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

// Constant obfuscation target
int magic_constant() {
    int secret = 0xDEADBEEF;  // Constant to obfuscate
    return (secret >> 16) ^ (secret & 0xFFFF);
}

// String encoding target
void print_secret() {
    const char *secret_msg = "This is a secret message!";
    printf("%s\n", secret_msg);
}

int main() {
    printf("=== LLVM IR Backend Test ===\n\n");

    // Test MBA transformations
    printf("--- MBA Tests ---\n");
    int a = 0x55, b = 0xAA;
    printf("mba_add(0x%X, 0x%X) = 0x%X (expect 0xFF)\n", a, b, mba_add(a, b));
    printf("mba_sub(0x%X, 0x%X) = 0x%X (expect 0xFFFFFFAB)\n", a, b, mba_sub(a, b));
    printf("mba_xor(0x%X, 0x%X) = 0x%X (expect 0xFF)\n", a, b, mba_xor(a, b));
    printf("mba_and(0x%X, 0x%X) = 0x%X (expect 0x0)\n", a, b, mba_and(a, b));
    printf("mba_or(0x%X, 0x%X) = 0x%X (expect 0xFF)\n", a, b, mba_or(a, b));
    printf("complex_math(10, 20, 30) = %d\n", complex_math(10, 20, 30));

    // Test control flow
    printf("\n--- Control Flow Tests ---\n");
    printf("grade(95) = %d (A)\n", calculate_grade(95));
    printf("grade(85) = %d (B)\n", calculate_grade(85));
    printf("grade(55) = %d (F)\n", calculate_grade(55));

    printf("day(3) = %s\n", day_name(3));
    printf("day(7) = %s\n", day_name(7));

    printf("fib(10) = %d\n", fibonacci(10));

    // Test constants
    printf("\n--- Constant Tests ---\n");
    printf("magic = 0x%X\n", magic_constant());

    // Test strings
    printf("\n--- String Tests ---\n");
    print_secret();

    printf("\n=== Test Complete ===\n");
    return 0;
}
