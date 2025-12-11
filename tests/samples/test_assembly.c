/*
 * Sample C code for testing Assembly backend (morphect-asm)
 *
 * Build and test:
 *   gcc -S -o test_assembly.s test_assembly.c
 *   ./build/bin/morphect-asm test_assembly.s obfuscated_assembly.s
 *   gcc obfuscated_assembly.s -o test_assembly_obf
 *   ./test_assembly_obf
 */

#include <stdio.h>

// Simple arithmetic - good for MBA testing
int add_numbers(int a, int b) {
    return a + b;
}

int subtract_numbers(int a, int b) {
    return a - b;
}

int bitwise_ops(int x, int y) {
    int result = x ^ y;       // XOR
    result = result | (x & y); // OR and AND
    return result;
}

// Simple control flow - good for anti-disassembly, junk bytes
int check_value(int val) {
    if (val > 100) {
        return 1;
    } else if (val > 50) {
        return 2;
    } else if (val > 0) {
        return 3;
    }
    return 0;
}

// Loop for dead code insertion targets
int sum_array(int *arr, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

// String for string encoding testing
void print_message(const char *msg) {
    printf("Message: %s\n", msg);
}

int main() {
    printf("=== Assembly Backend Test ===\n");

    // Test arithmetic
    int a = 42, b = 17;
    printf("add(%d, %d) = %d\n", a, b, add_numbers(a, b));
    printf("sub(%d, %d) = %d\n", a, b, subtract_numbers(a, b));
    printf("bitwise(%d, %d) = %d\n", a, b, bitwise_ops(a, b));

    // Test control flow
    printf("check(150) = %d\n", check_value(150));
    printf("check(75) = %d\n", check_value(75));
    printf("check(25) = %d\n", check_value(25));
    printf("check(-5) = %d\n", check_value(-5));

    // Test array sum
    int arr[] = {1, 2, 3, 4, 5};
    printf("sum([1,2,3,4,5]) = %d\n", sum_array(arr, 5));

    // Test string
    print_message("Hello from assembly test!");

    printf("=== Test Complete ===\n");
    return 0;
}
