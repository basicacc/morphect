/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_and.cpp - MBA transformation for AND operations
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

    // Pattern: %result = and <type> %a, %b
    std::regex and_pattern(
        R"((\s*)(%[\w.]+)\s*=\s*and\s+(\w+)\s+(%[\w.]+),\s*(%[\w.]+|[\d-]+))"
    );

    std::smatch match;
    if (!std::regex_match(line, match, and_pattern)) {
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string type = match[3];
    std::string op1 = match[4];
    std::string op2 = match[5];

    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) : selectVariant(config);

    static int temp_counter = 0;
    std::string t1 = "%mba_and_t" + std::to_string(temp_counter++);
    std::string t2 = "%mba_and_t" + std::to_string(temp_counter++);
    std::string t3 = "%mba_and_t" + std::to_string(temp_counter++);
    std::string t4 = "%mba_and_t" + std::to_string(temp_counter++);

    switch (var_idx) {
        case 0:  // (a | b) - (a ^ b)
        {
            // %t1 = or type %a, %b
            // %t2 = xor type %a, %b
            // %dest = sub type %t1, %t2
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t2);
            break;
        }
        case 1:  // ~(~a | ~b) - De Morgan's law
        {
            // %t1 = xor type %a, -1        ; ~a
            // %t2 = xor type %b, -1        ; ~b
            // %t3 = or type %t1, %t2       ; ~a | ~b
            // %dest = xor type %t3, -1     ; ~(~a | ~b)
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t3 + " = or " + type + " " + t1 + ", " + t2);
            result.push_back(indent + dest + " = xor " + type + " " + t3 + ", -1");
            break;
        }
        case 2:  // a - (a & ~b)
        {
            // %t1 = xor type %b, -1        ; ~b
            // %t2 = and type %a, %t1       ; a & ~b
            // %dest = sub type %a, %t2     ; a - (a & ~b)
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + dest + " = sub " + type + " " + op1 + ", " + t2);
            break;
        }
        case 3:  // (a | b) & ~(a ^ b)
        {
            // %t1 = or type %a, %b         ; a | b
            // %t2 = xor type %a, %b        ; a ^ b
            // %t3 = xor type %t2, -1       ; ~(a ^ b)
            // %dest = and type %t1, %t3
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = xor " + type + " " + t2 + ", -1");
            result.push_back(indent + dest + " = and " + type + " " + t1 + ", " + t3);
            break;
        }
        case 4:  // b - (~a & b)
        default:
        {
            // %t1 = xor type %a, -1        ; ~a
            // %t2 = and type %t1, %b       ; ~a & b
            // %dest = sub type %b, %t2     ; b - (~a & b)
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + t1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + op2 + ", " + t2);
            break;
        }
    }

    logger_.debug("Applied MBA AND variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
