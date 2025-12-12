/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_or.cpp - MBA transformation for OR operations
 *
 * Mathematical identities for a | b:
 *   a | b = (a ^ b) + (a & b)
 *   a | b = (a + b) - (a & b)
 *   a | b = ~(~a & ~b)              [De Morgan's law]
 *   a | b = a + (b & ~a)            [a plus bits only in b]
 *   a | b = b + (a & ~b)            [b plus bits only in a]
 *   a | b = (a & b) | (a ^ b)       [AND union XOR]
 *   a | b = a ^ (b & ~a)            [XOR with bits only in b]
 *   a | b = ~(~a ^ b) ^ b           [complex form]
 */

#include "mba_or.hpp"
#include <regex>
#include <sstream>

namespace morphect {
namespace mba {

bool MBAOr::applyGimple(void* gsi, void* stmt, int variant_idx,
                        const MBAConfig& config) {
    (void)gsi; (void)stmt; (void)variant_idx; (void)config;
    return false;
}

// Global counter for unique temp names
static int g_or_temp_counter = 500000;

/**
 * Apply MBA OR transformation to LLVM IR
 *
 * Input pattern:  %result = or <type> %a, %b
 * Output pattern: Multiple instructions implementing the MBA identity
 */
std::vector<std::string> LLVMMBAOr::applyIR(const std::string& line,
                                             int variant_idx,
                                             const MBAConfig& config) {
    std::vector<std::string> result;

    if (!shouldApply(config)) {
        return result;
    }

    // Robust pattern for OR - handles vector types like <4 x i32>
    std::regex or_pattern(
        R"(^(\s*)(%[\w.]+)\s*=\s*or\s+(<\d+\s*x\s*)?(\w+)(\s*>)?\s+(%[\w.]+),\s*(%[\w.]+|[\d-]+)\s*$)"
    );

    std::smatch match;
    if (!std::regex_match(line, match, or_pattern)) {
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string vec_prefix = match[3];
    std::string base_type = match[4];
    std::string vec_suffix = match[5];
    std::string op1 = match[6];
    std::string op2 = match[7];

    std::string type = vec_prefix + base_type + vec_suffix;

    // Skip constant OR
    if (op1[0] != '%' && op2[0] != '%') {
        return result;
    }

    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) % 8 : selectVariant(config);

    int base = g_or_temp_counter;
    g_or_temp_counter += 10;
    std::string t1 = "%_mba_o" + std::to_string(base);
    std::string t2 = "%_mba_o" + std::to_string(base + 1);
    std::string t3 = "%_mba_o" + std::to_string(base + 2);
    std::string t4 = "%_mba_o" + std::to_string(base + 3);
    std::string t5 = "%_mba_o" + std::to_string(base + 4);

    switch (var_idx) {
        case 0:  // (a ^ b) + (a & b)
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
            break;
        }
        case 1:  // (a + b) - (a & b)
        {
            result.push_back(indent + t1 + " = add " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t2);
            break;
        }
        case 2:  // ~(~a & ~b) - De Morgan's law
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t3 + " = and " + type + " " + t1 + ", " + t2);
            result.push_back(indent + dest + " = xor " + type + " " + t3 + ", -1");
            break;
        }
        case 3:  // a + (b & ~a)
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op2 + ", " + t1);
            result.push_back(indent + dest + " = add " + type + " " + op1 + ", " + t2);
            break;
        }
        case 4:  // b + (a & ~b)
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + dest + " = add " + type + " " + op2 + ", " + t2);
            break;
        }
        case 5:  // a ^ (b & ~a) - XOR with bits only in b
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op2 + ", " + t1);
            result.push_back(indent + dest + " = xor " + type + " " + op1 + ", " + t2);
            break;
        }
        case 6:  // (a | b) via nested: ((a ^ b) | a) - expands nicely
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + t1 + ", " + op2);  // bits only in b
            result.push_back(indent + dest + " = add " + type + " " + op1 + ", " + t2);
            break;
        }
        case 7:  // Complex: (a ^ b) + (a & b) - same as variant 0 but using different temp names
        default:
        {
            // a | b = (a ^ b) + (a & b) - verified correct identity
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);  // a ^ b
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);  // a & b
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);  // (a^b) + (a&b)
            break;
        }
    }

    logger_.debug("Applied MBA OR variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
