/*
 * mba_add.hpp - addition obfuscation
 *
 * a + b can be rewritten as:
 *   (a ^ b) + 2 * (a & b)
 *   (a | b) + (a & b)
 *   2 * (a | b) - (a ^ b)
 *   etc.
 */

#ifndef MORPHECT_MBA_ADD_HPP
#define MORPHECT_MBA_ADD_HPP

#include "mba_base.hpp"

namespace morphect {
namespace mba {

class MBAAdd : public GimpleMBATransformation {
public:
    MBAAdd() : GimpleMBATransformation("MBA_ADD") {}

    std::string getName() const override { return "MBA_ADD"; }
    std::string getOperation() const override { return "PLUS_EXPR"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("xor_and", "(a ^ b) + 2 * (a & b)", 0.25),
            MBAVariant("or_and", "(a | b) + (a & b)", 0.2),
            MBAVariant("or_xor", "2 * (a | b) - (a ^ b)", 0.15),
            MBAVariant("xor_and_shift", "(a ^ b) + ((a & b) << 1)", 0.1),
            MBAVariant("negate_sub", "a - (-b)", 0.15),
            MBAVariant("not_complement", "~(~a - b)", 0.15)
        };
    }

    // actual GIMPLE impl is in the plugin
    bool applyGimple(void* gsi, void* stmt, int variant_idx,
                    const MBAConfig& config) override;
};

class LLVMMBAAdd : public LLVMMBATransformation {
public:
    LLVMMBAAdd() : LLVMMBATransformation("MBA_ADD") {}

    std::string getName() const override { return "MBA_ADD"; }
    std::string getOperation() const override { return "add"; }

    std::vector<MBAVariant> getVariants() const override {
        return {
            MBAVariant("xor_and_shl", "(a ^ b) + 2 * (a & b)", 0.12),
            MBAVariant("or_and", "(a | b) + (a & b)", 0.12),
            MBAVariant("or_shl_xor", "2 * (a | b) - (a ^ b)", 0.10),
            MBAVariant("twos_comp", "a - (~b + 1)", 0.10),
            MBAVariant("complement", "~(~a - b)", 0.10),
            MBAVariant("symmetric", "(a & b) + (a | b)", 0.10),
            MBAVariant("complex_or", "((a ^ b) | (a & b)) + (a & b)", 0.10),
            MBAVariant("xor_and_chain", "(a ^ b) + (a & b) + (a & b)", 0.08),
            MBAVariant("shift_div", "(2*a + 2*b) >> 1", 0.08),
            MBAVariant("deep_nest", "~(~(a ^ b) - 2*(a & b))", 0.10)
        };
    }

    std::vector<std::string> applyIR(const std::string& line,
                                     int variant_idx,
                                     const MBAConfig& config) override;
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_ADD_HPP
