/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * string_decoder.c - Runtime string decoder
 *
 * This file provides runtime functions for decoding XOR-encoded strings.
 * Link this with your obfuscated program when string encoding is enabled.
 *
 * Usage:
 *   gcc -c string_decoder.c -o string_decoder.o
 *   gcc obfuscated.o string_decoder.o -o output
 */

#include <stdlib.h>
#include <string.h>

/**
 * Decode an XOR-encoded string
 *
 * @param encoded  Pointer to encoded bytes
 * @param length   Length of encoded string (not including null terminator)
 * @param key      XOR key used for encoding
 * @return         Newly allocated decoded string (caller must free)
 */
char* __morphect_decode_str(const unsigned char* encoded, size_t length, unsigned char key) {
    char* decoded = (char*)malloc(length + 1);
    if (!decoded) return NULL;

    for (size_t i = 0; i < length; i++) {
        decoded[i] = (char)(encoded[i] ^ key);
    }
    decoded[length] = '\0';

    return decoded;
}

/**
 * Decode an XOR-encoded string in place
 *
 * @param buffer   Buffer containing encoded bytes (will be modified)
 * @param length   Length of encoded string
 * @param key      XOR key used for encoding
 */
void __morphect_decode_str_inplace(char* buffer, size_t length, unsigned char key) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (char)((unsigned char)buffer[i] ^ key);
    }
}

/**
 * Decode with rolling XOR (each byte XORed with previous decoded byte)
 *
 * @param encoded  Pointer to encoded bytes
 * @param length   Length of encoded string
 * @param init_key Initial XOR key
 * @return         Newly allocated decoded string (caller must free)
 */
char* __morphect_decode_str_rolling(const unsigned char* encoded, size_t length, unsigned char init_key) {
    char* decoded = (char*)malloc(length + 1);
    if (!decoded) return NULL;

    unsigned char key = init_key;
    for (size_t i = 0; i < length; i++) {
        decoded[i] = (char)(encoded[i] ^ key);
        key = (unsigned char)decoded[i];
    }
    decoded[length] = '\0';

    return decoded;
}

/**
 * Legacy compatibility alias
 */
char* __decode_str(const unsigned char* encoded, size_t length, unsigned char key) {
    return __morphect_decode_str(encoded, length, key);
}

void __decode_str_inplace(char* buffer, size_t length, unsigned char key) {
    __morphect_decode_str_inplace(buffer, length, key);
}
