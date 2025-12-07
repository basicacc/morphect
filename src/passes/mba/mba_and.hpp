/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_and.hpp - MBA transformation for AND operations
 *
 * Mathematical identities for a & b:
 *   Variant 0: (a | b) - (a ^ b)
 *   Variant 1: ~(~a | ~b)              [De Morgan's law]
 *   Variant 2: a - (a & ~b)            [a minus bits only in a]
 *   Variant 3: (a | b) & ~(a ^ b)      [OR masked by NOT XOR]
 *   Variant 4: b - (~a & b)            [b minus bits only in b]
 */

#ifndef MORPHECT_MBA_AND_HPP
#define MORPHECT_MBA_AND_HPP

#include "mba_base.hpp"

namespace morphect {
namespace mba {

/**
 * AND transformation variants
 */
class MBAAnd : public GimpleMBATransformation {
public:
    MBAAnd() : GimpleMBATransformation("MBA_AND") {}

    std::string getName() const override { return "MBA_AND"; }
    std::string getOperation() const override { return "BIT_AND_EXPR"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("or_xor", "(a | b) - (a ^ b)", 0.25),
            MBAVariant("de_morgan", "~(~a | ~b)", 0.25),
            MBAVariant("diff_a", "a - (a & ~b)", 0.15),
            MBAVariant("or_not_xor", "(a | b) & ~(a ^ b)", 0.2),
            MBAVariant("diff_b", "b - (~a & b)", 0.15)
        };
    }

    bool applyGimple(void* gsi, void* stmt, int variant_idx,
                    const MBAConfig& config) override;
};

/**
 * LLVM IR AND transformation
 */
class LLVMMBAAnd : public LLVMMBATransformation {
public:
    LLVMMBAAnd() : LLVMMBATransformation("MBA_AND") {}

    std::string getName() const override { return "MBA_AND"; }
    std::string getOperation() const override { return "and"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("or_xor", "(a | b) - (a ^ b)", 0.25),
            MBAVariant("de_morgan", "~(~a | ~b)", 0.25),
            MBAVariant("diff_a", "a - (a & ~b)", 0.15),
            MBAVariant("or_not_xor", "(a | b) & ~(a ^ b)", 0.2),
            MBAVariant("diff_b", "b - (~a & b)", 0.15)
        };
    }

    std::vector<std::string> applyIR(const std::string& line,
                                     int variant_idx,
                                     const MBAConfig& config) override;
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_AND_HPP
