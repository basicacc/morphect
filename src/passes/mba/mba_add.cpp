/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_add.cpp - MBA transformation for addition operations
 *
 * This file contains the LLVM IR implementation.
 * GIMPLE implementation is in the GCC plugin directly due to
 * header dependencies.
 */

#include "mba_add.hpp"
#include <regex>
#include <sstream>

namespace morphect {
namespace mba {

// GIMPLE implementation is in gimple_obf_plugin.cpp due to GCC header deps
bool MBAAdd::applyGimple(void* gsi, void* stmt, int variant_idx,
                         const MBAConfig& config) {
    // This is implemented directly in the plugin
    // This stub exists for the interface
    (void)gsi; (void)stmt; (void)variant_idx; (void)config;
    return false;
}

/**
 * Apply MBA ADD transformation to LLVM IR
 *
 * Input pattern:  %result = add <type> %a, %b
 * Output pattern: Multiple instructions implementing the MBA identity
 */
std::vector<std::string> LLVMMBAAdd::applyIR(const std::string& line,
                                              int variant_idx,
                                              const MBAConfig& config) {
    std::vector<std::string> result;

    // Check if we should apply
    if (!shouldApply(config)) {
        return result;
    }

    // Pattern: %result = add <type> %a, %b
    // Also matches: %result = add nsw <type> %a, %b
    std::regex add_pattern(
        R"((\s*)(%[\w.]+)\s*=\s*add\s+(nsw\s+|nuw\s+|nsw nuw\s+)?(\w+)\s+(%[\w.]+|[\d-]+),\s*(%[\w.]+|[\d-]+))"
    );

    std::smatch match;
    if (!std::regex_match(line, match, add_pattern)) {
        return result;  // Not an add instruction
    }

    std::string indent = match[1];
    std::string dest = match[2];
    // std::string flags = match[3];  // nsw/nuw flags (ignored for now)
    std::string type = match[4];
    std::string op1 = match[5];
    std::string op2 = match[6];

    // Select variant
    size_t var_idx = (variant_idx >= 0) ?
        static_cast<size_t>(variant_idx) : selectVariant(config);

    // Generate unique temp names
    static int temp_counter = 0;
    std::string t1 = "%mba_t" + std::to_string(temp_counter++);
    std::string t2 = "%mba_t" + std::to_string(temp_counter++);
    std::string t3 = "%mba_t" + std::to_string(temp_counter++);

    switch (var_idx) {
        case 0:  // (a ^ b) + 2 * (a & b)
        {
            // %t1 = xor type %a, %b
            // %t2 = and type %a, %b
            // %t3 = shl type %t2, 1
            // %dest = add type %t1, %t3
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = shl " + type + " " + t2 + ", 1");
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t3);
            break;
        }
        case 1:  // (a | b) + (a & b)
        {
            // %t1 = or type %a, %b
            // %t2 = and type %a, %b
            // %dest = add type %t1, %t2
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
            break;
        }
        case 2:  // 2 * (a | b) - (a ^ b)
        {
            // %t1 = or type %a, %b
            // %t2 = shl type %t1, 1
            // %t3 = xor type %a, %b
            // %dest = sub type %t2, %t3
            result.push_back(indent + t1 + " = or " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = shl " + type + " " + t1 + ", 1");
            result.push_back(indent + t3 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + t2 + ", " + t3);
            break;
        }
        case 3:  // (a ^ b) + ((a & b) << 1) - same as 0, explicit shift
        {
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t2 + " = and " + type + " " + op1 + ", " + op2);
            result.push_back(indent + t3 + " = shl " + type + " " + t2 + ", 1");
            result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t3);
            break;
        }
        case 4:  // a - (-b)  [negate and subtract]
        {
            // %t1 = sub type 0, %b     ; -b
            // %dest = sub type %a, %t1 ; a - (-b)
            result.push_back(indent + t1 + " = sub " + type + " 0, " + op2);
            result.push_back(indent + dest + " = sub " + type + " " + op1 + ", " + t1);
            break;
        }
        case 5:  // ~(~a - b)  [complement identity]
        default:
        {
            // %t1 = xor type %a, -1    ; ~a
            // %t2 = sub type %t1, %b   ; ~a - b
            // %dest = xor type %t2, -1 ; ~(~a - b)
            result.push_back(indent + t1 + " = xor " + type + " " + op1 + ", -1");
            result.push_back(indent + t2 + " = sub " + type + " " + t1 + ", " + op2);
            result.push_back(indent + dest + " = xor " + type + " " + t2 + ", -1");
            break;
        }
    }

    logger_.debug("Applied MBA ADD variant {} to: {}", var_idx, line);
    return result;
}

} // namespace mba
} // namespace morphect
