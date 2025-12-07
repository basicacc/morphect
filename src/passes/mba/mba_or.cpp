/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_or.cpp - MBA transformation for OR operations
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

    // Pattern: %result = or <type> %a, %b
    std::regex or_pattern(
        R"((\s*)(%[\w.]+)\s*=\s*or\s+(\w+)\s+(%[\w.]+),\s*(%[\w.]+|[\d-]+))"
    );

    std::smatch match;
    if (!std::regex_match(line, match, or_pattern)) {
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
    std::string t1 = "%mba_or_t" + std::to_string(temp_counter++);
    std::string t2 = "%mba_or_t" + std::to_string(temp_counter++);
    std::string t3 = "%mba_or_t" + std::to_string(temp_counter++);
    std::string t4 = "%mba_or_t" + std::to_string(temp_counter++);

    switch (var_idx) {
        case 0:  // (a ^ b) + (a & b)
        {
            // %t1 = xor type %a, %b
            // %t2 = and type %a, %b
            // %dest = add type %t1, %t2
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
            break;
        }
        case 1:  // (a + b) - (a & b)
        {
            // %t1 = add type %a, %b
            // %t2 = and type %a, %b
            // %dest = sub type %t1, %t2
            result.push_back(indent + t1 + " = add " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t2);
            break;
        }
        case 2:  // ~(~a & ~b) - De Morgan's law
        {
            // %t1 = xor type %a, -1        ; ~a
            // %t2 = xor type %b, -1        ; ~b
            // %t3 = and type %t1, %t2      ; ~a & ~b
            // %dest = xor type %t3, -1     ; ~(~a & ~b)
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t3 + " = and " + type + " " + t1 + ", " + t2);
            result.push_back(indent + dest + " = xor " + type + " " + t3 + ", -1");
            break;
        }
        case 3:  // a + (b & ~a)
        {
            // %t1 = xor type %a, -1        ; ~a
            // %t2 = and type %b, %t1       ; b & ~a
            // %dest = add type %a, %t2     ; a + (b & ~a)
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op2 + ", " + t1);
            result.push_back(indent + dest + " = add " + type + " " + op1 + ", " + t2);
            break;
        }
        case 4:  // b + (a & ~b)
        {
            // %t1 = xor type %b, -1        ; ~b
            // %t2 = and type %a, %t1       ; a & ~b
            // %dest = add type %b, %t2     ; b + (a & ~b)
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + dest + " = add " + type + " " + op2 + ", " + t2);
            break;
        }
        case 5:  // (a & b) | (a ^ b)
        default:
        {
            // %t1 = and type %a, %b        ; a & b
            // %t2 = xor type %a, %b        ; a ^ b
            // %dest = or type %t1, %t2     ; (a & b) | (a ^ b)
            result.push_back(indent + t1 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = or " + type + " " + t1 + ", " + t2);
            break;
        }
    }

    logger_.debug("Applied MBA OR variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
