/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_add.cpp - MBA transformation for addition operations
 *
 * This file contains the LLVM IR implementation with multiple variants
 * for maximum obfuscation complexity.
 *
 * Mathematical identities used:
 *   a + b = (a ^ b) + 2*(a & b)
 *   a + b = (a | b) + (a & b)
 *   a + b = 2*(a | b) - (a ^ b)
 *   a + b = a - (~b + 1)  [negate via two's complement]
 *   a + b = ~(~a - b)     [complement identity]
 *   a + b = (a & b) + (a | b)  [symmetric form]
 *   a + b = ((a ^ b) | (a & b)) + (a & b)  [complex form]
 */

#include "mba_add.hpp"
#include <regex>
#include <sstream>

namespace morphect {
namespace mba {

// GIMPLE implementation is in gimple_obf_plugin.cpp due to GCC header deps
bool MBAAdd::applyGimple(void* gsi, void* stmt, int variant_idx,
                         const MBAConfig& config) {
    (void)gsi; (void)stmt; (void)variant_idx; (void)config;
    return false;
}

// Global temp counter for unique names
static int g_add_temp_counter = 100000;

/**
 * Apply MBA ADD transformation to LLVM IR
 *
 * Supports: i8, i16, i32, i64, and vector types
 */
std::vector<std::string> LLVMMBAAdd::applyIR(const std::string& line,
                                              int variant_idx,
                                              const MBAConfig& config) {
    std::vector<std::string> result;

    if (!shouldApply(config)) {
        return result;
    }

    // More robust pattern that handles:
    // - Standard: %result = add i32 %a, %b
    // - With flags: %result = add nsw i32 %a, %b
    // - With nuw nsw: %result = add nuw nsw i32 %a, %b
    // - Vector types: %result = add <4 x i32> %a, %b
    std::regex add_pattern(
        R"(^(\s*)(%[\w.]+)\s*=\s*add\s+(nsw\s+|nuw\s+|nuw nsw\s+|nsw nuw\s+)?(<\d+\s*x\s*)?(\w+)(\s*>)?\s+(%[\w.]+|[\d-]+),\s*(%[\w.]+|[\d-]+)\s*$)"
    );

    std::smatch match;
    if (!std::regex_match(line, match, add_pattern)) {
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string flags = match[3];  // nsw/nuw flags
    std::string vec_prefix = match[4];  // <4 x  for vectors
    std::string base_type = match[5];   // i32, i64, etc
    std::string vec_suffix = match[6];  // > for vectors
    std::string op1 = match[7];
    std::string op2 = match[8];

    // Reconstruct full type
    std::string type = vec_prefix + base_type + vec_suffix;

    // Skip if both operands are constants (optimizer will fold anyway)
    bool op1_const = (op1[0] != '%');
    bool op2_const = (op2[0] != '%');
    if (op1_const && op2_const) {
        return result;
    }

    // Select variant (we have 10 variants now)
    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) % 10 : selectVariant(config);

    // Generate unique temp names with counter to avoid collisions
    int base = g_add_temp_counter;
    g_add_temp_counter += 10;
    std::string t1 = "%_mba_a" + std::to_string(base);
    std::string t2 = "%_mba_a" + std::to_string(base + 1);
    std::string t3 = "%_mba_a" + std::to_string(base + 2);
    std::string t4 = "%_mba_a" + std::to_string(base + 3);
    std::string t5 = "%_mba_a" + std::to_string(base + 4);
    std::string t6 = "%_mba_a" + std::to_string(base + 5);

    switch (var_idx) {
        case 0:  // (a ^ b) + 2 * (a & b) - classic MBA
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = shl " + type + " " + t2 + ", 1");
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t3);
            break;
        }
        case 1:  // (a | b) + (a & b)
        {
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
            break;
        }
        case 2:  // 2 * (a | b) - (a ^ b)
        {
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = shl " + type + " " + t1 + ", 1");
            result.push_back(indent + t3 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t2 + ", " + t3);
            break;
        }
        case 3:  // a - (~b + 1) - two's complement negation
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = add " + type + " " + t1 + ", 1");
            result.push_back(indent + dest + " = sub " + type + " " + op1 + ", " + t2);
            break;
        }
        case 4:  // ~(~a - b) - complement identity
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = sub " + type + " " + t1 + ", " + op2);
            result.push_back(indent + dest + " = xor " + type + " " + t2 + ", -1");
            break;
        }
        case 5:  // (a & b) + (a | b) - symmetric form
        {
            result.push_back(indent + t1 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
            break;
        }
        case 6:  // ((a ^ b) | (a & b)) + (a & b) - complex
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = or " + type + " " + t1 + ", " + t2);
            result.push_back(indent + dest + " = add " + type + " " + t3 + ", " + t2);
            break;
        }
        case 7:  // (a | b) + (a & b) with intermediate XOR obfuscation
        {
            // a|b = (a^b) + (a&b), so a+b = (a^b) + 2*(a&b)
            // But we compute it differently for variety
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = add " + type + " " + t1 + ", " + t2);  // a|b
            result.push_back(indent + dest + " = add " + type + " " + t3 + ", " + t2); // +a&b
            break;
        }
        case 8:  // (2*a + 2*b) / 2 but done with shifts to confuse
        {
            result.push_back(indent + t1 + " = shl " + type + " " + op1 + ", 1");
            result.push_back(indent + t2 + " = shl " + type + " " + op2 + ", 1");
            result.push_back(indent + t3 + " = add " + type + " " + t1 + ", " + t2);
            result.push_back(indent + dest + " = lshr " + type + " " + t3 + ", 1");
            break;
        }
        case 9:  // Deep nesting: ~(~(a ^ b) - 2*(a & b))
        default:
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + t1 + ", -1");  // ~(a^b)
            result.push_back(indent + t3 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t4 + " = shl " + type + " " + t3 + ", 1");   // 2*(a&b)
            result.push_back(indent + t5 + " = sub " + type + " " + t2 + ", " + t4);
            result.push_back(indent + dest + " = xor " + type + " " + t5 + ", -1");
            break;
        }
    }

    logger_.debug("Applied MBA ADD variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
