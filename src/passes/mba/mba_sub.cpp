/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_sub.cpp - MBA transformation for subtraction operations
 *
 * Mathematical identities used:
 *   a - b = a + (~b + 1)           [two's complement]
 *   a - b = ~(~a + b)              [complement identity]
 *   a - b = (a ^ b) - 2*(~a & b)   [MBA form]
 *   a - b = (a & ~b) - (~a & b)    [bitwise decomposition]
 *   a - b = (a | ~b) - ~(a & b)    [DeMorgan variant]
 */

#include "mba_sub.hpp"
#include <regex>
#include <sstream>

namespace morphect {
namespace mba {

bool MBASub::applyGimple(void* gsi, void* stmt, int variant_idx,
                         const MBAConfig& config) {
    (void)gsi; (void)stmt; (void)variant_idx; (void)config;
    return false;
}

// Global temp counter
static int g_sub_temp_counter = 200000;

/**
 * Apply MBA SUB transformation to LLVM IR
 */
std::vector<std::string> LLVMMBASub::applyIR(const std::string& line,
                                              int variant_idx,
                                              const MBAConfig& config) {
    std::vector<std::string> result;

    if (!shouldApply(config)) {
        return result;
    }

    // Robust pattern matching
    std::regex sub_pattern(
        R"(^(\s*)(%[\w.]+)\s*=\s*sub\s+(nsw\s+|nuw\s+|nuw nsw\s+|nsw nuw\s+)?(<\d+\s*x\s*)?(\w+)(\s*>)?\s+(%[\w.]+|[\d-]+),\s*(%[\w.]+|[\d-]+)\s*$)"
    );

    std::smatch match;
    if (!std::regex_match(line, match, sub_pattern)) {
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string vec_prefix = match[4];
    std::string base_type = match[5];
    std::string vec_suffix = match[6];
    std::string op1 = match[7];
    std::string op2 = match[8];

    std::string type = vec_prefix + base_type + vec_suffix;

    // Skip constant-constant
    if (op1[0] != '%' && op2[0] != '%') {
        return result;
    }

    // Skip "sub type 0, X" (negation) - don't transform further
    if (op1 == "0") {
        return result;
    }

    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) % 8 : selectVariant(config);

    int base = g_sub_temp_counter;
    g_sub_temp_counter += 10;
    std::string t1 = "%_mba_s" + std::to_string(base);
    std::string t2 = "%_mba_s" + std::to_string(base + 1);
    std::string t3 = "%_mba_s" + std::to_string(base + 2);
    std::string t4 = "%_mba_s" + std::to_string(base + 3);
    std::string t5 = "%_mba_s" + std::to_string(base + 4);
    std::string t6 = "%_mba_s" + std::to_string(base + 5);

    switch (var_idx) {
        case 0:  // a + (~b + 1) - two's complement
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = add " + type + " " + t1 + ", 1");
            result.push_back(indent + dest + " = add " + type + " " + op1 + ", " + t2);
            break;
        }
        case 1:  // ~(~a + b) - complement identity
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = add " + type + " " + t1 + ", " + op2);
            result.push_back(indent + dest + " = xor " + type + " " + t2 + ", -1");
            break;
        }
        case 2:  // (a ^ b) - 2 * (~a & b)
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t3 + " = and " + type + " " + t2 + ", " + op2);
            result.push_back(indent + t4 + " = shl " + type + " " + t3 + ", 1");
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t4);
            break;
        }
        case 3:  // (a & ~b) - (~a & b)
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + t3 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t4 + " = and " + type + " " + t3 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t2 + ", " + t4);
            break;
        }
        case 4:  // (a | ~b) + ~(a | b) + 1 - complex
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = or " + type + " " + op1 + ", " + t1);
            result.push_back(indent + t3 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t4 + " = xor " + type + " " + t3 + ", -1");
            result.push_back(indent + t5 + " = add " + type + " " + t2 + ", " + t4);
            result.push_back(indent + dest + " = add " + type + " " + t5 + ", 1");
            break;
        }
        case 5:  // (a ^ ~b) + 2*(a & ~b) + 1
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", " + t1);
            result.push_back(indent + t3 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + t4 + " = shl " + type + " " + t3 + ", 1");
            result.push_back(indent + t5 + " = add " + type + " " + t2 + ", " + t4);
            result.push_back(indent + dest + " = add " + type + " " + t5 + ", 1");
            break;
        }
        case 6:  // a + (b ^ -1) + 1 - simple two's comp variant
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = add " + type + " " + op1 + ", " + t1);
            result.push_back(indent + dest + " = add " + type + " " + t2 + ", 1");
            break;
        }
        case 7:  // ~(~a + b) via nested ops
        default:
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t3 + " = xor " + type + " " + t2 + ", -1");  // b again
            result.push_back(indent + t4 + " = add " + type + " " + t1 + ", " + t3);
            result.push_back(indent + dest + " = xor " + type + " " + t4 + ", -1");
            break;
        }
    }

    logger_.debug("Applied MBA SUB variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
