/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * data_base.hpp - Base definitions for data obfuscation passes
 *
 * Data obfuscation includes:
 *   - String encoding (XOR, rolling XOR, custom)
 *   - Constant obfuscation (split, arithmetic, encode)
 */

#ifndef MORPHECT_DATA_BASE_HPP
#define MORPHECT_DATA_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <cstdint>

namespace morphect {
namespace data {

/**
 * String encoding methods
 */
enum class StringEncodingMethod {
    XOR,            // Simple XOR with single key
    RollingXOR,     // XOR where key = previous decoded byte
    MultiByteXOR,   // XOR with multi-byte key
    Base64XOR,      // Base64 encode then XOR
    Custom,         // User-defined encoding
    // Enhanced methods (Phase 3.5)
    Split,          // Split into multiple parts, concatenate at runtime
    ChainedXOR,     // XOR with computed key chain
    ByteSwapXOR,    // Swap adjacent bytes then XOR
    RC4             // RC4 stream cipher encoding
};

/**
 * Configuration for string encoding
 */
struct StringEncodingConfig {
    bool enabled = true;
    StringEncodingMethod method = StringEncodingMethod::XOR;
    uint8_t xor_key = 0x7B;                    // Default XOR key
    std::vector<uint8_t> multi_byte_key;      // For multi-byte XOR
    int min_string_length = 3;                 // Minimum string length to encode
    std::string decoder_function = "__morphect_decode_str";
    std::vector<std::string> exclude_patterns; // Patterns to exclude (e.g., "usage:")
    bool encode_format_strings = false;        // Encode printf format strings?
    // Enhanced options (Phase 3.5)
    int max_split_parts = 4;                   // Max parts for Split method
    int min_split_length = 8;                  // Min string length for splitting
    bool use_delayed_decoding = false;         // Decode strings only when accessed
    bool randomize_method = false;             // Randomly choose encoding method per string
    std::vector<uint8_t> rc4_key;              // Key for RC4 encoding
};

/**
 * Constant obfuscation strategies
 */
enum class ConstantObfStrategy {
    XOR,            // c = (c ^ key) ^ key
    Split,          // c = c1 + c2 where c1 + c2 = c
    Arithmetic,     // c = (c + offset) - offset
    MultiplyDivide, // c = (c * factor) / factor
    BitSplit,       // Split into multiple bit operations
    // Enhanced strategies (Phase 3.4)
    MBA,            // c = MBA expression (e.g., (a^b) + 2*(a&b))
    MultiSplit,     // c = c1 + c2 + c3 + ... (multi-variable)
    NestedXOR,      // c = ((c ^ k1) ^ k2) ^ k3
    ShiftAdd        // c = (a << n) + b
};

/**
 * Configuration for constant obfuscation
 */
struct ConstantObfConfig {
    bool enabled = true;
    double probability = 0.7;
    std::vector<ConstantObfStrategy> strategies = {
        ConstantObfStrategy::XOR,
        ConstantObfStrategy::Split,
        ConstantObfStrategy::Arithmetic
    };
    int64_t min_value = 2;       // Don't obfuscate 0, 1, -1
    int64_t max_value = INT64_MAX;
    bool obfuscate_zero = false;
    bool obfuscate_one = false;
    // Enhanced options (Phase 3.4)
    int max_multi_split_parts = 4;   // Max parts for MultiSplit strategy
    int max_nested_xor_depth = 3;    // Max depth for NestedXOR strategy
    bool use_mba_expressions = true; // Use MBA for constant generation
    bool handle_large_constants = true;  // Support large constants (>10000)
    bool handle_floating_point = true;   // Support float/double constants
};

/**
 * Floating point obfuscation result
 */
struct ObfuscatedFloat {
    ConstantObfStrategy strategy;
    double original;
    std::string expression;
    // For bit manipulation approach, we use integer representation
    uint64_t int_bits;        // IEEE 754 bit representation
    std::vector<int64_t> parts;
};

/**
 * Encoded string result
 */
struct EncodedString {
    std::vector<uint8_t> encoded_bytes;
    uint8_t key;
    std::string original;
    size_t length;
    StringEncodingMethod method = StringEncodingMethod::XOR;

    /**
     * Generate C array initializer
     */
    std::string toCArrayInit() const {
        std::string result = "{";
        for (size_t i = 0; i < encoded_bytes.size(); i++) {
            if (i > 0) result += ", ";
            result += std::to_string(encoded_bytes[i]);
        }
        result += "}";
        return result;
    }

    /**
     * Generate hex string representation
     */
    std::string toHexString() const {
        std::string result;
        const char* hex = "0123456789abcdef";
        for (uint8_t b : encoded_bytes) {
            result += "\\x";
            result += hex[b >> 4];
            result += hex[b & 0xF];
        }
        return result;
    }
};

/**
 * Split string result - for string splitting encoding
 */
struct SplitString {
    std::string original;
    std::vector<std::string> parts;           // The split parts
    std::vector<EncodedString> encoded_parts; // Each part encoded separately
    std::vector<size_t> split_points;         // Where the string was split

    /**
     * Get number of parts
     */
    size_t numParts() const { return parts.size(); }
};

/**
 * Delayed decode info - for lazy string decoding
 */
struct DelayedDecodeInfo {
    std::string var_name;          // Original variable name
    EncodedString encoded;         // The encoded string
    bool decoded = false;          // Whether it has been decoded
    std::string decoded_var_name;  // Variable name after decoding
};

/**
 * Obfuscated constant result
 */
struct ObfuscatedConstant {
    ConstantObfStrategy strategy;
    int64_t original;
    std::string expression;  // The obfuscated expression
    std::vector<int64_t> parts;  // Component values
};

} // namespace data
} // namespace morphect

#endif // MORPHECT_DATA_BASE_HPP
