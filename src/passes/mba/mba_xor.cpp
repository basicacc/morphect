/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_xor.cpp - MBA transformation for XOR operations
 */

#include "mba_xor.hpp"
#include <regex>
#include <sstream>

namespace morphect {
namespace mba {

bool MBAXor::applyGimple(void* gsi, void* stmt, int variant_idx,
                         const MBAConfig& config) {
    (void)gsi; (void)stmt; (void)variant_idx; (void)config;
    return false;
}

/**
 * Apply MBA XOR transformation to LLVM IR
 *
 * Input pattern:  %result = xor <type> %a, %b
 * Output pattern: Multiple instructions implementing the MBA identity
 */
std::vector<std::string> LLVMMBAXor::applyIR(const std::string& line,
                                              int variant_idx,
                                              const MBAConfig& config) {
    std::vector<std::string> result;

    if (!shouldApply(config)) {
        return result;
    }

    // Pattern: %result = xor <type> %a, %b
    // Skip if XOR with -1 (that's a NOT operation)
    std::regex xor_pattern(
        R"((\s*)(%[\w.]+)\s*=\s*xor\s+(\w+)\s+(%[\w.]+),\s*(%[\w.]+|[\d-]+))"
    );

    std::smatch match;
    if (!std::regex_match(line, match, xor_pattern)) {
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string type = match[3];
    std::string op1 = match[4];
    std::string op2 = match[5];

    // Skip XOR with -1 (NOT operation) - don't transform
    if (op2 == "-1") {
        return result;
    }

    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) : selectVariant(config);

    static int temp_counter = 0;
    std::string t1 = "%mba_xor_t" + std::to_string(temp_counter++);
    std::string t2 = "%mba_xor_t" + std::to_string(temp_counter++);
    std::string t3 = "%mba_xor_t" + std::to_string(temp_counter++);
    std::string t4 = "%mba_xor_t" + std::to_string(temp_counter++);
    std::string t5 = "%mba_xor_t" + std::to_string(temp_counter++);

    switch (var_idx) {
        case 0:  // (a | b) - (a & b)
        {
            // %t1 = or type %a, %b
            // %t2 = and type %a, %b
            // %dest = sub type %t1, %t2
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t2);
            break;
        }
        case 1:  // (a + b) - 2 * (a & b)
        {
            // %t1 = add type %a, %b
            // %t2 = and type %a, %b
            // %t3 = shl type %t2, 1
            // %dest = sub type %t1, %t3
            result.push_back(indent + t1 + " = add " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = shl " + type + " " + t2 + ", 1");
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t3);
            break;
        }
        case 2:  // (a | b) & ~(a & b)
        {
            // %t1 = or type %a, %b
            // %t2 = and type %a, %b
            // %t3 = xor type %t2, -1       ; ~(a & b)
            // %dest = and type %t1, %t3
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = xor " + type + " " + t2 + ", -1");
            result.push_back(indent + dest + " = and " + type + " " + t1 + ", " + t3);
            break;
        }
        case 3:  // (~a & b) | (a & ~b)
        {
            // %t1 = xor type %a, -1        ; ~a
            // %t2 = and type %t1, %b       ; ~a & b
            // %t3 = xor type %b, -1        ; ~b
            // %t4 = and type %a, %t3       ; a & ~b
            // %dest = or type %t2, %t4
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + t1 + ", " + op2);
            result.push_back(indent + t3 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t4 + " = and " + type + " " + op1 + ", " + t3);
            result.push_back(indent + dest + " = or " + type + " " + t2 + ", " + t4);
            break;
        }
        case 4:  // (a | b) & (~a | ~b)
        {
            // %t1 = or type %a, %b         ; a | b
            // %t2 = xor type %a, -1        ; ~a
            // %t3 = xor type %b, -1        ; ~b
            // %t4 = or type %t2, %t3       ; ~a | ~b
            // %dest = and type %t1, %t4
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t3 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t4 + " = or " + type + " " + t2 + ", " + t3);
            result.push_back(indent + dest + " = and " + type + " " + t1 + ", " + t4);
            break;
        }
        case 5:  // (a | b) ^ (a & b)
        default:
        {
            // %t1 = or type %a, %b         ; a | b
            // %t2 = and type %a, %b        ; a & b
            // %dest = xor type %t1, %t2    ; (a | b) ^ (a & b)
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = xor " + type + " " + t1 + ", " + t2);
            break;
        }
    }

    logger_.debug("Applied MBA XOR variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
