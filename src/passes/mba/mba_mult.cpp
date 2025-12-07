/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_mult.cpp - MBA transformation for multiplication operations
 */

#include "mba_mult.hpp"
#include <regex>
#include <sstream>
#include <cstdlib>

namespace morphect {
namespace mba {

bool MBAMult::applyGimple(void* gsi, void* stmt, int variant_idx,
                          const MBAConfig& config) {
    (void)gsi; (void)stmt; (void)variant_idx; (void)config;
    return false;
}

/**
 * Apply MBA MULT transformation to LLVM IR
 *
 * Input pattern:  %result = mul <type> %a, <constant>
 * Output pattern: Shift/add operations
 */
std::vector<std::string> LLVMMBAMult::applyIR(const std::string& line,
                                               int variant_idx,
                                               const MBAConfig& config) {
    std::vector<std::string> result;

    if (!shouldApply(config)) {
        return result;
    }

    // Pattern: %result = mul <type> %a, <constant>
    // We only transform when one operand is a constant
    std::regex mul_pattern(
        R"((\s*)(%[\w.]+)\s*=\s*mul\s+(nsw\s+|nuw\s+|nsw nuw\s+)?(\w+)\s+(%[\w.]+),\s*([\d-]+))"
    );

    std::smatch match;
    if (!std::regex_match(line, match, mul_pattern)) {
        // Try with constant first
        std::regex mul_pattern2(
            R"((\s*)(%[\w.]+)\s*=\s*mul\s+(nsw\s+|nuw\s+|nsw nuw\s+)?(\w+)\s+([\d-]+),\s*(%[\w.]+))"
        );
        if (!std::regex_match(line, match, mul_pattern2)) {
            return result;
        }
        // Swap operands for pattern2
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string type = match[4];
    std::string var_op = match[5];
    std::string const_str = match[6];

    // Parse constant
    int64_t constant;
    try {
        constant = std::stoll(const_str);
    } catch (...) {
        return result;  // Can't parse constant
    }

    // Handle negative numbers or zero
    if (constant <= 0) {
        return result;  // Don't transform negative or zero multipliers
    }

    static int temp_counter = 0;

    // Select transformation based on constant value
    if (isPowerOf2(constant)) {
        // Variant 0: a * 2^n = a << n
        int shift = log2(constant);
        result.push_back(indent + dest + " = shl " + type + " " + var_op +
                        ", " + std::to_string(shift));
        logger_.debug("Applied MBA MULT shift variant (x{}) to: {}", constant, line);
    }
    else if (isPowerOf2(constant + 1)) {
        // Variant 1: a * (2^n - 1) = (a << n) - a
        // e.g., a * 7 = (a << 3) - a
        int shift = log2(constant + 1);
        std::string t1 = "%mba_mul_t" + std::to_string(temp_counter++);
        result.push_back(indent + t1 + " = shl " + type + " " + var_op +
                        ", " + std::to_string(shift));
        result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + var_op);
        logger_.debug("Applied MBA MULT shift-sub variant (x{}) to: {}", constant, line);
    }
    else if (isPowerOf2(constant - 1)) {
        // a * (2^n + 1) = (a << n) + a
        // e.g., a * 9 = (a << 3) + a
        int shift = log2(constant - 1);
        std::string t1 = "%mba_mul_t" + std::to_string(temp_counter++);
        result.push_back(indent + t1 + " = shl " + type + " " + var_op +
                        ", " + std::to_string(shift));
        result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + var_op);
        logger_.debug("Applied MBA MULT shift-add variant (x{}) to: {}", constant, line);
    }
    else if (constant <= 8) {
        // Variant 3: Small constants - use addition chain
        // a * 3 = a + a + a (or better: (a << 1) + a)
        // a * 5 = (a << 2) + a
        // a * 6 = (a << 2) + (a << 1)

        switch (constant) {
            case 3: {
                std::string t1 = "%mba_mul_t" + std::to_string(temp_counter++);
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + var_op);
                break;
            }
            case 5: {
                std::string t1 = "%mba_mul_t" + std::to_string(temp_counter++);
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 2");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + var_op);
                break;
            }
            case 6: {
                std::string t1 = "%mba_mul_t" + std::to_string(temp_counter++);
                std::string t2 = "%mba_mul_t" + std::to_string(temp_counter++);
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 2");
                result.push_back(indent + t2 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
                break;
            }
            default:
                return result;  // Don't transform
        }
        logger_.debug("Applied MBA MULT add-chain variant (x{}) to: {}", constant, line);
    }
    else {
        // Variant 2: Decompose into power-of-2 components
        // e.g., a * 10 = (a << 3) + (a << 1) = a*8 + a*2
        std::vector<int> shifts;
        int64_t remaining = constant;
        int bit_pos = 0;

        while (remaining > 0) {
            if (remaining & 1) {
                shifts.push_back(bit_pos);
            }
            remaining >>= 1;
            bit_pos++;
        }

        if (shifts.size() > 4) {
            return result;  // Too many additions, not worth it
        }

        std::vector<std::string> terms;
        for (int shift : shifts) {
            std::string term;
            if (shift == 0) {
                term = var_op;
            } else {
                term = "%mba_mul_t" + std::to_string(temp_counter++);
                result.push_back(indent + term + " = shl " + type + " " +
                                var_op + ", " + std::to_string(shift));
            }
            terms.push_back(term);
        }

        // Sum all terms
        std::string current = terms[0];
        for (size_t i = 1; i < terms.size(); i++) {
            std::string next_sum;
            if (i == terms.size() - 1) {
                next_sum = dest;
            } else {
                next_sum = "%mba_mul_t" + std::to_string(temp_counter++);
            }
            result.push_back(indent + next_sum + " = add " + type + " " +
                            current + ", " + terms[i]);
            current = next_sum;
        }

        logger_.debug("Applied MBA MULT decompose variant (x{}) to: {}", constant, line);
    }

    return result;
}

} // namespace mba
} // namespace morphect
