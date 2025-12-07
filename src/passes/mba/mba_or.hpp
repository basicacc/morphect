/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_or.hpp - MBA transformation for OR operations
 *
 * Mathematical identities for a | b:
 *   Variant 0: (a ^ b) + (a & b)
 *   Variant 1: (a + b) - (a & b)
 *   Variant 2: ~(~a & ~b)              [De Morgan's law]
 *   Variant 3: a + (b & ~a)            [a plus bits only in b]
 *   Variant 4: b + (a & ~b)            [b plus bits only in a]
 *   Variant 5: (a & b) | (a ^ b)       [AND union XOR]
 */

#ifndef MORPHECT_MBA_OR_HPP
#define MORPHECT_MBA_OR_HPP

#include "mba_base.hpp"

namespace morphect {
namespace mba {

/**
 * OR transformation variants
 */
class MBAOr : public GimpleMBATransformation {
public:
    MBAOr() : GimpleMBATransformation("MBA_OR") {}

    std::string getName() const override { return "MBA_OR"; }
    std::string getOperation() const override { return "BIT_IOR_EXPR"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("xor_and", "(a ^ b) + (a & b)", 0.2),
            MBAVariant("add_and", "(a + b) - (a & b)", 0.2),
            MBAVariant("de_morgan", "~(~a & ~b)", 0.2),
            MBAVariant("sum_diff_a", "a + (b & ~a)", 0.13),
            MBAVariant("sum_diff_b", "b + (a & ~b)", 0.13),
            MBAVariant("and_xor", "(a & b) | (a ^ b)", 0.14)
        };
    }

    bool applyGimple(void* gsi, void* stmt, int variant_idx,
                    const MBAConfig& config) override;
};

/**
 * LLVM IR OR transformation
 */
class LLVMMBAOr : public LLVMMBATransformation {
public:
    LLVMMBAOr() : LLVMMBATransformation("MBA_OR") {}

    std::string getName() const override { return "MBA_OR"; }
    std::string getOperation() const override { return "or"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("xor_and", "(a ^ b) + (a & b)", 0.2),
            MBAVariant("add_and", "(a + b) - (a & b)", 0.2),
            MBAVariant("de_morgan", "~(~a & ~b)", 0.2),
            MBAVariant("sum_diff_a", "a + (b & ~a)", 0.13),
            MBAVariant("sum_diff_b", "b + (a & ~b)", 0.13),
            MBAVariant("and_xor", "(a & b) | (a ^ b)", 0.14)
        };
    }

    std::vector<std::string> applyIR(const std::string& line,
                                     int variant_idx,
                                     const MBAConfig& config) override;
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_OR_HPP
