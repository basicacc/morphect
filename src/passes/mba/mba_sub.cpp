/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_sub.cpp - MBA transformation for subtraction operations
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

/**
 * Apply MBA SUB transformation to LLVM IR
 *
 * Input pattern:  %result = sub <type> %a, %b
 * Output pattern: Multiple instructions implementing the MBA identity
 */
std::vector<std::string> LLVMMBASub::applyIR(const std::string& line,
                                              int variant_idx,
                                              const MBAConfig& config) {
    std::vector<std::string> result;

    if (!shouldApply(config)) {
        return result;
    }

    // Pattern: %result = sub <type> %a, %b
    std::regex sub_pattern(
        R"((\s*)(%[\w.]+)\s*=\s*sub\s+(nsw\s+|nuw\s+|nsw nuw\s+)?(\w+)\s+(%[\w.]+|[\d-]+),\s*(%[\w.]+|[\d-]+))"
    );

    std::smatch match;
    if (!std::regex_match(line, match, sub_pattern)) {
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string type = match[4];
    std::string op1 = match[5];
    std::string op2 = match[6];

    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) : selectVariant(config);

    static int temp_counter = 0;
    std::string t1 = "%mba_sub_t" + std::to_string(temp_counter++);
    std::string t2 = "%mba_sub_t" + std::to_string(temp_counter++);
    std::string t3 = "%mba_sub_t" + std::to_string(temp_counter++);
    std::string t4 = "%mba_sub_t" + std::to_string(temp_counter++);
    std::string t5 = "%mba_sub_t" + std::to_string(temp_counter++);

    switch (var_idx) {
        case 0:  // (a ^ b) - 2 * (~a & b)
        {
            // %t1 = xor type %a, %b
            // %t2 = xor type %a, -1        ; ~a
            // %t3 = and type %t2, %b       ; ~a & b
            // %t4 = shl type %t3, 1        ; 2 * (~a & b)
            // %dest = sub type %t1, %t4
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t3 + " = and " + type + " " + t2 + ", " + op2);
            result.push_back(indent + t4 + " = shl " + type + " " + t3 + ", 1");
            result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t4);
            break;
        }
        case 1:  // (a | ~b) - (~a | b) + 1
        {
            // %t1 = xor type %b, -1        ; ~b
            // %t2 = or type %a, %t1        ; a | ~b
            // %t3 = xor type %a, -1        ; ~a
            // %t4 = or type %t3, %b        ; ~a | b
            // %t5 = sub type %t2, %t4
            // %dest = add type %t5, 1
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = or " + type + " " + op1 + ", " + t1);
            result.push_back(indent + t3 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t4 + " = or " + type + " " + t3 + ", " + op2);
            result.push_back(indent + t5 + " = sub " + type + " " + t2 + ", " + t4);
            result.push_back(indent + dest + " = add " + type + " " + t5 + ", 1");
            break;
        }
        case 2:  // a + (~b + 1) - two's complement
        {
            // %t1 = xor type %b, -1        ; ~b
            // %t2 = add type %t1, 1        ; ~b + 1 = -b
            // %dest = add type %a, %t2     ; a + (-b) = a - b
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = add " + type + " " + t1 + ", 1");
            result.push_back(indent + dest + " = add " + type + " " + op1 + ", " + t2);
            break;
        }
        case 3:  // ~(~a + b)
        {
            // %t1 = xor type %a, -1        ; ~a
            // %t2 = add type %t1, %b       ; ~a + b
            // %dest = xor type %t2, -1     ; ~(~a + b)
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = add " + type + " " + t1 + ", " + op2);
            result.push_back(indent + dest + " = xor " + type + " " + t2 + ", -1");
            break;
        }
        case 4:  // (a & ~b) - (~a & b)
        default:
        {
            // %t1 = xor type %b, -1        ; ~b
            // %t2 = and type %a, %t1       ; a & ~b
            // %t3 = xor type %a, -1        ; ~a
            // %t4 = and type %t3, %b       ; ~a & b
            // %dest = sub type %t2, %t4    ; (a & ~b) - (~a & b)
            result.push_back(indent + t1 + " = xor " + type + " " + op2 + ", -1");
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + t1);
            result.push_back(indent + t3 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t4 + " = and " + type + " " + t3 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t2 + ", " + t4);
            break;
        }
    }

    logger_.debug("Applied MBA SUB variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
