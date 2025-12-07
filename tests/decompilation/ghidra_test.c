/**
 * Morphect - Ghidra Decompilation Test
 *
 * This test program is designed to verify that obfuscated code is
 * resistant to decompilation while maintaining correct functionality.
 *
 * The test includes various code patterns:
 * - Simple arithmetic (MBA targets)
 * - Control flow with conditionals
 * - Loops
 * - Function calls
 * - Switch statements
 *
 * Compile normally:
 *   gcc -O0 ghidra_test.c -o ghidra_test_normal
 *
 * Compile with obfuscation:
 *   gcc -O0 -fplugin=./morphect_plugin.so ghidra_test.c -o ghidra_test_obfuscated
 *
 * Then use Ghidra to analyze both binaries and compare decompilation results.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Magic constants that should be obscured by obfuscation
#define SECRET_KEY 0xDEADBEEF
#define ROUND_CONSTANT 0x5A5A5A5A

/**
 * Simple XOR cipher - good target for MBA obfuscation
 * The XOR operations should be transformed to equivalent expressions
 */
void xor_cipher(unsigned char* data, int len, unsigned int key) {
    for (int i = 0; i < len; i++) {
        data[i] = data[i] ^ ((key >> ((i % 4) * 8)) & 0xFF);
    }
}

/**
 * Simple hash function with multiple arithmetic operations
 * Tests MBA on: ADD, XOR, AND, OR
 */
unsigned int simple_hash(const char* str) {
    unsigned int hash = 0;
    unsigned int c;

    while ((c = *str++)) {
        // These operations should all be MBA transformed
        hash = hash + c;           // ADD
        hash = hash ^ (c << 5);    // XOR
        hash = hash | (c >> 2);    // OR
        hash = (hash & 0xFFFFFF) + (hash >> 24);  // AND
    }

    return hash;
}

/**
 * Control flow function - tests CFF obfuscation
 * Multiple branches and a loop should be flattened
 */
int check_license(const char* key) {
    if (key == NULL) {
        return -1;
    }

    int len = strlen(key);

    // Length check
    if (len < 8 || len > 32) {
        return -2;
    }

    // Checksum validation
    int checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum += key[i];
        checksum ^= (i * 31);
    }

    // Magic value check
    if ((checksum & 0xFF) != 0x42) {
        return -3;
    }

    // Complex validation with nested conditions
    int valid = 1;
    for (int i = 0; i < len - 1; i++) {
        int diff = key[i+1] - key[i];
        if (diff < -10 || diff > 10) {
            valid = 0;
            break;
        }
        if (key[i] < 'A' || key[i] > 'z') {
            valid = 0;
            break;
        }
    }

    return valid ? 0 : -4;
}

/**
 * Switch statement test - should be integrated into CFF
 */
int process_command(int cmd, int arg1, int arg2) {
    int result = 0;

    switch (cmd) {
        case 1:
            result = arg1 + arg2;  // ADD
            break;
        case 2:
            result = arg1 - arg2;  // SUB
            break;
        case 3:
            result = arg1 * arg2;  // MUL
            break;
        case 4:
            if (arg2 != 0) {
                result = arg1 / arg2;  // DIV
            }
            break;
        case 5:
            result = arg1 ^ arg2;  // XOR
            break;
        case 6:
            result = arg1 & arg2;  // AND
            break;
        case 7:
            result = arg1 | arg2;  // OR
            break;
        case 8:
            // Complex case with multiple operations
            result = (arg1 + arg2) ^ (arg1 - arg2);
            result = result & ((arg1 | arg2) + ROUND_CONSTANT);
            break;
        default:
            result = -1;
            break;
    }

    return result;
}

/**
 * Recursive function - tests obfuscation with recursion
 */
int fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;

    int a = fibonacci(n - 1);
    int b = fibonacci(n - 2);

    // This add should be MBA transformed
    return a + b;
}

/**
 * State machine - complex control flow
 */
typedef enum {
    STATE_INIT,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_ERROR,
    STATE_DONE
} State;

int state_machine(int* events, int num_events) {
    State state = STATE_INIT;
    int result = 0;

    for (int i = 0; i < num_events; i++) {
        int event = events[i];

        switch (state) {
            case STATE_INIT:
                if (event == 1) {
                    state = STATE_RUNNING;
                    result += 10;
                } else if (event == -1) {
                    state = STATE_ERROR;
                }
                break;

            case STATE_RUNNING:
                if (event == 2) {
                    state = STATE_PAUSED;
                } else if (event == 3) {
                    state = STATE_DONE;
                    result += 100;
                } else if (event < 0) {
                    state = STATE_ERROR;
                } else {
                    result += event;
                }
                break;

            case STATE_PAUSED:
                if (event == 1) {
                    state = STATE_RUNNING;
                } else if (event == -1) {
                    state = STATE_ERROR;
                }
                break;

            case STATE_ERROR:
                result = -1;
                return result;

            case STATE_DONE:
                // Stay in done state
                break;
        }
    }

    if (state != STATE_DONE && state != STATE_ERROR) {
        result = -2;
    }

    return result;
}

/**
 * Encryption round function - complex bitwise operations
 */
unsigned int encrypt_round(unsigned int block, unsigned int key) {
    unsigned int temp;

    // These should all be MBA transformed
    temp = block ^ key;
    temp = ((temp << 5) | (temp >> 27)) ^ ROUND_CONSTANT;
    temp = temp + (key & 0xFFFF);
    temp = temp ^ ((key >> 16) | (key << 16));

    return temp;
}

/**
 * Main test function - verifies correct behavior
 */
int main(int argc, char* argv[]) {
    int errors = 0;

    // Test 1: XOR cipher
    unsigned char test_data[] = "Hello, World!";
    int data_len = strlen((char*)test_data);
    unsigned char original[32];
    memcpy(original, test_data, data_len + 1);

    xor_cipher(test_data, data_len, SECRET_KEY);
    xor_cipher(test_data, data_len, SECRET_KEY);  // Double XOR should restore

    if (memcmp(test_data, original, data_len) != 0) {
        printf("FAIL: XOR cipher\n");
        errors++;
    } else {
        printf("PASS: XOR cipher\n");
    }

    // Test 2: Hash consistency
    unsigned int hash1 = simple_hash("test string");
    unsigned int hash2 = simple_hash("test string");

    if (hash1 != hash2) {
        printf("FAIL: Hash consistency\n");
        errors++;
    } else {
        printf("PASS: Hash consistency (hash=0x%08x)\n", hash1);
    }

    // Test 3: License check
    int license_result = check_license(NULL);
    if (license_result != -1) {
        printf("FAIL: License NULL check\n");
        errors++;
    } else {
        printf("PASS: License NULL check\n");
    }

    // Test 4: Process command
    if (process_command(1, 10, 5) != 15) {
        printf("FAIL: Command ADD\n");
        errors++;
    } else {
        printf("PASS: Command ADD\n");
    }

    if (process_command(5, 0xFF, 0x0F) != 0xF0) {
        printf("FAIL: Command XOR\n");
        errors++;
    } else {
        printf("PASS: Command XOR\n");
    }

    // Test 5: Fibonacci
    int fib_result = fibonacci(10);
    if (fib_result != 55) {
        printf("FAIL: Fibonacci (got %d, expected 55)\n", fib_result);
        errors++;
    } else {
        printf("PASS: Fibonacci\n");
    }

    // Test 6: State machine
    int events1[] = {1, 5, 5, 5, 3};
    int sm_result1 = state_machine(events1, 5);
    if (sm_result1 != 125) {  // 10 + 5 + 5 + 5 + 100
        printf("FAIL: State machine 1 (got %d, expected 125)\n", sm_result1);
        errors++;
    } else {
        printf("PASS: State machine 1\n");
    }

    int events2[] = {1, -1};  // Start then error
    int sm_result2 = state_machine(events2, 2);
    if (sm_result2 != -1) {
        printf("FAIL: State machine 2 (got %d, expected -1)\n", sm_result2);
        errors++;
    } else {
        printf("PASS: State machine 2\n");
    }

    // Test 7: Encryption round
    unsigned int encrypted = encrypt_round(0x12345678, SECRET_KEY);
    // Just verify it's deterministic
    unsigned int encrypted2 = encrypt_round(0x12345678, SECRET_KEY);
    if (encrypted != encrypted2) {
        printf("FAIL: Encryption round consistency\n");
        errors++;
    } else {
        printf("PASS: Encryption round (result=0x%08x)\n", encrypted);
    }

    // Summary
    printf("\n=== Summary ===\n");
    if (errors == 0) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("%d test(s) FAILED\n", errors);
        return 1;
    }
}
