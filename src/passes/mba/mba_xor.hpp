/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_xor.hpp - MBA transformation for XOR operations
 *
 * Mathematical identities for a ^ b:
 *   Variant 0: (a | b) - (a & b)
 *   Variant 1: (a + b) - 2 * (a & b)
 *   Variant 2: (a | b) & ~(a & b)
 *   Variant 3: (~a & b) | (a & ~b)
 *   Variant 4: (a | b) & (~a | ~b)
 *   Variant 5: (a | b) ^ (a & b)
 */

#ifndef MORPHECT_MBA_XOR_HPP
#define MORPHECT_MBA_XOR_HPP

#include "mba_base.hpp"

namespace morphect {
namespace mba {

/**
 * XOR transformation variants
 */
class MBAXor : public GimpleMBATransformation {
public:
    MBAXor() : GimpleMBATransformation("MBA_XOR") {}

    std::string getName() const override { return "MBA_XOR"; }
    std::string getOperation() const override { return "BIT_XOR_EXPR"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("or_and", "(a | b) - (a & b)", 0.2),
            MBAVariant("add_and", "(a + b) - 2 * (a & b)", 0.2),
            MBAVariant("or_nand", "(a | b) & ~(a & b)", 0.15),
            MBAVariant("not_and", "(~a & b) | (a & ~b)", 0.15),
            MBAVariant("or_not_or", "(a | b) & (~a | ~b)", 0.15),
            MBAVariant("or_xor_and", "(a | b) ^ (a & b)", 0.15)
        };
    }

    bool applyGimple(void* gsi, void* stmt, int variant_idx,
                    const MBAConfig& config) override;
};

/**
 * LLVM IR XOR transformation
 */
class LLVMMBAXor : public LLVMMBATransformation {
public:
    LLVMMBAXor() : LLVMMBATransformation("MBA_XOR") {}

    std::string getName() const override { return "MBA_XOR"; }
    std::string getOperation() const override { return "xor"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("or_and", "(a | b) - (a & b)", 0.2),
            MBAVariant("add_and", "(a + b) - 2 * (a & b)", 0.2),
            MBAVariant("or_nand", "(a | b) & ~(a & b)", 0.15),
            MBAVariant("not_and", "(~a & b) | (a & ~b)", 0.15),
            MBAVariant("or_not_or", "(a | b) & (~a | ~b)", 0.15),
            MBAVariant("or_xor_and", "(a | b) ^ (a & b)", 0.15)
        };
    }

    std::vector<std::string> applyIR(const std::string& line,
                                     int variant_idx,
                                     const MBAConfig& config) override;
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_XOR_HPP
