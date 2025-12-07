/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_mult.hpp - MBA transformation for multiplication operations
 *
 * Multiplication is harder to transform with pure boolean algebra,
 * but we can use:
 *   Variant 0: Shift-and-add for power-of-2 multipliers
 *   Variant 1: (a << n) - a for (2^n - 1) multipliers
 *   Variant 2: Decomposition: a * b = a * (b1 + b2) = a*b1 + a*b2
 *   Variant 3: Loop-based multiplication (very obfuscated but slower)
 *
 * Note: These apply when one operand is a constant.
 * For variable * variable, we use strength reduction patterns.
 */

#ifndef MORPHECT_MBA_MULT_HPP
#define MORPHECT_MBA_MULT_HPP

#include "mba_base.hpp"

namespace morphect {
namespace mba {

/**
 * MULT transformation variants
 */
class MBAMult : public GimpleMBATransformation {
public:
    MBAMult() : GimpleMBATransformation("MBA_MULT") {}

    std::string getName() const override { return "MBA_MULT"; }
    std::string getOperation() const override { return "MULT_EXPR"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("shift_add", "a << log2(b) [if b is power of 2]", 0.4),
            MBAVariant("shift_sub", "(a << n) - a [if b = 2^n - 1]", 0.3),
            MBAVariant("decompose", "a * b1 + a * b2 [split constant]", 0.2),
            MBAVariant("add_chain", "a + a + ... + a [small constants]", 0.1)
        };
    }

    bool applyGimple(void* gsi, void* stmt, int variant_idx,
                    const MBAConfig& config) override;

    /**
     * Check if a number is a power of 2
     */
    static bool isPowerOf2(int64_t n) {
        return n > 0 && (n & (n - 1)) == 0;
    }

    /**
     * Get log2 of a power of 2
     */
    static int log2(int64_t n) {
        int result = 0;
        while (n > 1) {
            n >>= 1;
            result++;
        }
        return result;
    }
};

/**
 * LLVM IR MULT transformation
 */
class LLVMMBAMult : public LLVMMBATransformation {
public:
    LLVMMBAMult() : LLVMMBATransformation("MBA_MULT") {}

    std::string getName() const override { return "MBA_MULT"; }
    std::string getOperation() const override { return "mul"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("shift_add", "a << log2(b) [if b is power of 2]", 0.4),
            MBAVariant("shift_sub", "(a << n) - a [if b = 2^n - 1]", 0.3),
            MBAVariant("decompose", "a * b1 + a * b2 [split constant]", 0.2),
            MBAVariant("add_chain", "a + a + ... + a [small constants]", 0.1)
        };
    }

    std::vector<std::string> applyIR(const std::string& line,
                                     int variant_idx,
                                     const MBAConfig& config) override;

    static bool isPowerOf2(int64_t n) {
        return n > 0 && (n & (n - 1)) == 0;
    }

    static int log2(int64_t n) {
        int result = 0;
        while (n > 1) {
            n >>= 1;
            result++;
        }
        return result;
    }
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_MULT_HPP
