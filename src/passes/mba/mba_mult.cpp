/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_mult.cpp - MBA transformation for multiplication operations
 *
 * Transforms constant multiplications into shift/add sequences:
 *   a * 2^n = a << n
 *   a * (2^n - 1) = (a << n) - a
 *   a * (2^n + 1) = (a << n) + a
 *   a * k = sum of (a << bit_position) for each set bit in k
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

// Global counter for unique temp names
static int g_mult_temp_counter = 600000;

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

    // Robust pattern for MUL - handles vector types and flags
    // Pattern 1: %result = mul [flags] <type> %a, <constant>
    std::regex mul_pattern(
        R"(^(\s*)(%[\w.]+)\s*=\s*mul\s+(nsw\s+|nuw\s+|nuw nsw\s+|nsw nuw\s+)?(<\d+\s*x\s*)?(\w+)(\s*>)?\s+(%[\w.]+),\s*([\d-]+)\s*$)"
    );

    std::smatch match;
    bool swapped = false;
    if (!std::regex_match(line, match, mul_pattern)) {
        // Try with constant first
        std::regex mul_pattern2(
            R"(^(\s*)(%[\w.]+)\s*=\s*mul\s+(nsw\s+|nuw\s+|nuw nsw\s+|nsw nuw\s+)?(<\d+\s*x\s*)?(\w+)(\s*>)?\s+([\d-]+),\s*(%[\w.]+)\s*$)"
        );
        if (!std::regex_match(line, match, mul_pattern2)) {
            return result;
        }
        swapped = true;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string vec_prefix = match[4];
    std::string base_type = match[5];
    std::string vec_suffix = match[6];
    std::string var_op = swapped ? match[8].str() : match[7].str();
    std::string const_str = swapped ? match[7].str() : match[8].str();

    std::string type = vec_prefix + base_type + vec_suffix;

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

    // Get unique temp base
    int base = g_mult_temp_counter;
    g_mult_temp_counter += 10;

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
        std::string t1 = "%_mba_m" + std::to_string(base);
        result.push_back(indent + t1 + " = shl " + type + " " + var_op +
                        ", " + std::to_string(shift));
        result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + var_op);
        logger_.debug("Applied MBA MULT shift-sub variant (x{}) to: {}", constant, line);
    }
    else if (isPowerOf2(constant - 1)) {
        // a * (2^n + 1) = (a << n) + a
        // e.g., a * 9 = (a << 3) + a
        int shift = log2(constant - 1);
        std::string t1 = "%_mba_m" + std::to_string(base);
        result.push_back(indent + t1 + " = shl " + type + " " + var_op +
                        ", " + std::to_string(shift));
        result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + var_op);
        logger_.debug("Applied MBA MULT shift-add variant (x{}) to: {}", constant, line);
    }
    else if (constant <= 15) {
        // Small constants - use optimized addition chains
        // These are hand-optimized for minimum instruction count
        int tidx = 0;
        auto getTemp = [&]() { return "%_mba_m" + std::to_string(base + tidx++); };

        switch (constant) {
            case 3: {
                // a * 3 = (a << 1) + a
                std::string t1 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + var_op);
                break;
            }
            case 5: {
                // a * 5 = (a << 2) + a
                std::string t1 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 2");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + var_op);
                break;
            }
            case 6: {
                // a * 6 = (a << 2) + (a << 1)
                std::string t1 = getTemp();
                std::string t2 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 2");
                result.push_back(indent + t2 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
                break;
            }
            case 7: {
                // a * 7 = (a << 3) - a
                std::string t1 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 3");
                result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + var_op);
                break;
            }
            case 9: {
                // a * 9 = (a << 3) + a
                std::string t1 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 3");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + var_op);
                break;
            }
            case 10: {
                // a * 10 = (a << 3) + (a << 1)
                std::string t1 = getTemp();
                std::string t2 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 3");
                result.push_back(indent + t2 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
                break;
            }
            case 11: {
                // a * 11 = (a << 3) + (a << 1) + a
                std::string t1 = getTemp();
                std::string t2 = getTemp();
                std::string t3 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 3");
                result.push_back(indent + t2 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + t3 + " = add " + type + " " + t1 + ", " + t2);
                result.push_back(indent + dest + " = add " + type + " " + t3 + ", " + var_op);
                break;
            }
            case 12: {
                // a * 12 = (a << 3) + (a << 2)
                std::string t1 = getTemp();
                std::string t2 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 3");
                result.push_back(indent + t2 + " = shl " + type + " " + var_op + ", 2");
                result.push_back(indent + dest + " = add " + type + " " + t1 + ", " + t2);
                break;
            }
            case 13: {
                // a * 13 = (a << 4) - (a << 1) - a
                std::string t1 = getTemp();
                std::string t2 = getTemp();
                std::string t3 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 4");
                result.push_back(indent + t2 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + t3 + " = sub " + type + " " + t1 + ", " + t2);
                result.push_back(indent + dest + " = sub " + type + " " + t3 + ", " + var_op);
                break;
            }
            case 14: {
                // a * 14 = (a << 4) - (a << 1)
                std::string t1 = getTemp();
                std::string t2 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 4");
                result.push_back(indent + t2 + " = shl " + type + " " + var_op + ", 1");
                result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + t2);
                break;
            }
            case 15: {
                // a * 15 = (a << 4) - a
                std::string t1 = getTemp();
                result.push_back(indent + t1 + " = shl " + type + " " + var_op + ", 4");
                result.push_back(indent + dest + " = sub " + type + " " + t1 + ", " + var_op);
                break;
            }
            default:
                return result;  // Don't transform
        }
        logger_.debug("Applied MBA MULT add-chain variant (x{}) to: {}", constant, line);
    }
    else {
        // General case: Decompose into power-of-2 components
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

        if (shifts.size() > 5) {
            return result;  // Too many additions, not worth it
        }

        int tidx = 0;
        std::vector<std::string> terms;
        for (int shift : shifts) {
            std::string term;
            if (shift == 0) {
                term = var_op;
            } else {
                term = "%_mba_m" + std::to_string(base + tidx++);
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
                next_sum = "%_mba_m" + std::to_string(base + tidx++);
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
