/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_sub.hpp - MBA transformation for subtraction operations
 *
 * Mathematical identities for a - b:
 *   Variant 0: (a ^ b) - 2 * (~a & b)
 *   Variant 1: (a | ~b) - (~a | b) + 1  [requires ~b, ~a]
 *   Variant 2: a + (~b + 1)  [two's complement]
 *   Variant 3: ~(~a + b)     [complement identity]
 *   Variant 4: (a & ~b) - (~a & b)  [diff of exclusive bits]
 */

#ifndef MORPHECT_MBA_SUB_HPP
#define MORPHECT_MBA_SUB_HPP

#include "mba_base.hpp"

namespace morphect {
namespace mba {

/**
 * SUB transformation variants
 */
class MBASub : public GimpleMBATransformation {
public:
    MBASub() : GimpleMBATransformation("MBA_SUB") {}

    std::string getName() const override { return "MBA_SUB"; }
    std::string getOperation() const override { return "MINUS_EXPR"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("xor_not_and", "(a ^ b) - 2 * (~a & b)", 0.25),
            MBAVariant("or_complement", "(a | ~b) - (~a | b) + 1", 0.2),
            MBAVariant("twos_complement", "a + (~b + 1)", 0.2),
            MBAVariant("not_add", "~(~a + b)", 0.2),
            MBAVariant("diff_exclusive", "(a & ~b) - (~a & b)", 0.15)
        };
    }

    bool applyGimple(void* gsi, void* stmt, int variant_idx,
                    const MBAConfig& config) override;
};

/**
 * LLVM IR SUB transformation
 */
class LLVMMBASub : public LLVMMBATransformation {
public:
    LLVMMBASub() : LLVMMBATransformation("MBA_SUB") {}

    std::string getName() const override { return "MBA_SUB"; }
    std::string getOperation() const override { return "sub"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("xor_not_and", "(a ^ b) - 2 * (~a & b)", 0.25),
            MBAVariant("or_complement", "(a | ~b) - (~a | b) + 1", 0.2),
            MBAVariant("twos_complement", "a + (~b + 1)", 0.2),
            MBAVariant("not_add", "~(~a + b)", 0.2),
            MBAVariant("diff_exclusive", "(a & ~b) - (~a & b)", 0.15)
        };
    }

    std::vector<std::string> applyIR(const std::string& line,
                                     int variant_idx,
                                     const MBAConfig& config) override;
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_SUB_HPP
