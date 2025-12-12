/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_and.cpp - MBA transformation for AND operations
 *
 * Mathematical identities for a & b:
 *   a & b = (a | b) - (a ^ b)
 *   a & b = ~(~a | ~b)              [De Morgan's law]
 *   a & b = a - (a & ~b)            [subtract bits only in a]
 *   a & b = (a | b) & ~(a ^ b)      [AND with mask of equal bits]
 *   a & b = b - (~a & b)            [subtract bits only in b]
 *   a & b = (a + b) - (a | b)       [arithmetic relationship]
 *   a & b = ~(~a | ~b)              [NAND negated]
 *   a & b = a ^ (a & ~b)            [XOR with bits only in a]
 */

#include "mba_and.hpp"
#include <regex>
#include <sstream>

namespace morphect {
namespace mba {

bool MBAAnd::applyGimple(void* gsi, void* stmt, int variant_idx,
                         const MBAConfig& config) {
    (void)gsi; (void)stmt; (void)variant_idx; (void)config;
    return false;
}

// Global counter for unique temp names
static int g_and_counter = 400000;

/**
 * Apply MBA AND transformation to LLVM IR
 *
 * Input pattern:  %result = and <type> %a, %b
 * Output pattern: Multiple instructions implementing the MBA identity
 */
std::vector<std::string> LLVMMBAAnd::applyIR(const std::string& line,
                                              int variant_idx,
                                              const MBAConfig& config) {
    std::vector<std::string> result;

    if (!shouldApply(config)) {
        return result;
    }

    // Robust pattern for AND - handles vector types like <4 x i32>
    std::regex and_pattern(
        R"(^(\s*)(%[\w.]+)\s*=\s*and\s+(<\d+\s*x\s*)?(\w+)(\s*>)?\s+(%[\w.]+),\s*(%[\w.]+|[\d-]+)\s*$)"
    );

    std::smatch match;
    if (!std::regex_match(line, match, and_pattern)) {
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

    // Skip constant AND
    if (op1[0] != '%' && op2[0] != '%') {
        return result;
    }

    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) % 8 : selectVariant(config);

    int base = g_and_counter;
    g_and_counter += 10;
    std::string t1 = "%_mba_n" + std::to_string(base);
    std::string t2 = "%_mba_n" + std::to_string(base + 1);
    std::string t3 = "%_mba_n" + std::to_string(base + 2);
    std::string t4 = "%_mba_n" + std::to_string(base + 3);
    std::string t5 = "%_mba_n" + std::to_string(base + 4);

    switch (var_idx) {
        case 0:  // (a | b) - (a ^ b)
        {
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t2);
            break;
        }
        case 1:  // ~(~a | ~b) - De Morgan's law
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t3 + " = or " + type + " " + t1 + ", " + t2);
            result.push_back(indent + dest + " = xor " + type + " " + t3 + ", -1");
            break;
        }
        case 2:  // a - (a & ~b)
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + dest + " = sub " + type + " " + op1 + ", " + t2);
            break;
        }
        case 3:  // (a | b) & ~(a ^ b)
        {
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = xor " + type + " " + t2 + ", -1");
            result.push_back(indent + dest + " = and " + type + " " + t1 + ", " + t3);
            break;
        }
        case 4:  // b - (~a & b)
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + t1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + op2 + ", " + t2);
            break;
        }
        case 5:  // (a + b) - (a | b) - arithmetic relationship
        {
            result.push_back(indent + t1 + " = add " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t2);
            break;
        }
        case 6:  // a ^ (a & ~b) - XOR with bits only in a
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + dest + " = xor " + type + " " + op1 + ", " + t2);
            break;
        }
        case 7:  // Complex via: (a|b) - ((~a & b) | (a & ~b)) = (a|b) - (a^b) = a&b
        default:
        {
            // Compute a^b the hard way, then subtract from a|b
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");  // ~a
            result.push_back(indent + t2 + " = and " + type + " " + t1 + ", " + op2);  // ~a & b
            result.push_back(indent + t3 + " = xor " + type + " " + op2 + ", -1");  // ~b
            result.push_back(indent + t4 + " = and " + type + " " + op1 + ", " + t3);  // a & ~b
            result.push_back(indent + t5 + " = or " + type + " " + t2 + ", " + t4);  // (~a & b) | (a & ~b) = a ^ b
            std::string t6 = "%_mba_n" + std::to_string(base + 5);
            result.push_back(indent + t6 + " = or " + type + " " + op1 + ", " + op2);  // a | b
            result.push_back(indent + dest + " = sub " + type + " " + t6 + ", " + t5);  // (a|b) - (a^b) = a&b
            break;
        }
    }

    logger_.debug("Applied MBA AND variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
