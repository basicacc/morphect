/**
 * Simple test program for Morphect obfuscation
 */

#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int xor_op(int a, int b) {
    return a ^ b;
}

int and_op(int a, int b) {
    return a & b;
}

int or_op(int a, int b) {
    return a | b;
}

int main() {
    int x = 10;
    int y = 5;

    int sum = add(x, y);         // 15
    int xor_result = xor_op(x, y);  // 15
    int and_result = and_op(x, y);  // 0
    int or_result = or_op(x, y);    // 15

    // Calculate final result
    int result = sum + xor_result - and_result + or_result;
    // 15 + 15 - 0 + 15 = 45

    printf("Result: %d\n", result);

    return result;
}
