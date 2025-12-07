/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_base.hpp - Base definitions for MBA (Mixed Boolean Arithmetic) transformations
 *
 * MBA transforms simple arithmetic/bitwise operations into mathematically
 * equivalent but more complex expressions using boolean algebra identities.
 */

#ifndef MORPHECT_MBA_BASE_HPP
#define MORPHECT_MBA_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <functional>

namespace morphect {
namespace mba {

/**
 * MBA variant descriptor
 * Each operation can have multiple equivalent transformations
 */
struct MBAVariant {
    std::string name;           // Human-readable name
    std::string expression;     // Mathematical expression (for documentation)
    double probability;         // Selection probability (weights)

    MBAVariant(const std::string& n, const std::string& expr, double prob = 1.0)
        : name(n), expression(expr), probability(prob) {}
};

/**
 * MBA transformation configuration
 */
struct MBAConfig {
    bool enabled = true;
    double probability = 0.85;      // Probability to apply transformation
    int nesting_depth = 1;          // How many times to nest transformations
    bool use_all_variants = false;  // Cycle through all variants vs random
    std::vector<double> variant_weights;  // Custom weights for variants
};

/**
 * Abstract base class for MBA transformations
 * Each operation (ADD, SUB, XOR, etc.) implements this
 */
class MBATransformation {
public:
    virtual ~MBATransformation() = default;

    /**
     * Get the name of this transformation (e.g., "MBA_ADD")
     */
    virtual std::string getName() const = 0;

    /**
     * Get the operation this transforms (e.g., PLUS_EXPR)
     */
    virtual std::string getOperation() const = 0;

    /**
     * Get all available variants for this transformation
     */
    virtual std::vector<MBAVariant> getVariants() const = 0;

    /**
     * Get the number of variants available
     */
    size_t getVariantCount() const { return getVariants().size(); }

    /**
     * Select a variant based on configuration
     */
    size_t selectVariant(const MBAConfig& config) const {
        const auto& variants = getVariants();
        if (variants.empty()) return 0;

        if (config.use_all_variants) {
            // Round-robin selection
            static size_t counter = 0;
            return (counter++) % variants.size();
        }

        // Weighted random selection
        std::vector<double> weights;
        if (!config.variant_weights.empty() &&
            config.variant_weights.size() == variants.size()) {
            weights = config.variant_weights;
        } else {
            for (const auto& v : variants) {
                weights.push_back(v.probability);
            }
        }

        return GlobalRandom::chooseWeightedIndex(weights);
    }

    /**
     * Check if transformation should be applied based on probability
     */
    bool shouldApply(const MBAConfig& config) const {
        if (!config.enabled) return false;
        return GlobalRandom::decide(config.probability);
    }

protected:
    Logger logger_;

    MBATransformation(const std::string& name) : logger_(name) {}
};

/**
 * GIMPLE-specific MBA transformation
 * Works with GCC's GIMPLE IR
 */
class GimpleMBATransformation : public MBATransformation {
public:
    GimpleMBATransformation(const std::string& name) : MBATransformation(name) {}

    /**
     * Apply the transformation to a GIMPLE statement
     *
     * @param gsi GIMPLE statement iterator (positioned at statement to transform)
     * @param stmt The statement to transform
     * @param variant_idx Which variant to use (or -1 for random)
     * @param config Transformation configuration
     * @return true if transformation was applied
     */
    virtual bool applyGimple(void* gsi, void* stmt, int variant_idx,
                            const MBAConfig& config) = 0;
};

/**
 * LLVM IR-specific MBA transformation
 * Works with LLVM IR text format
 */
class LLVMMBATransformation : public MBATransformation {
public:
    LLVMMBATransformation(const std::string& name) : MBATransformation(name) {}

    /**
     * Apply the transformation to an LLVM IR line
     *
     * @param line The IR line to transform
     * @param variant_idx Which variant to use (or -1 for random)
     * @param config Transformation configuration
     * @return Transformed line(s), or empty if not applicable
     */
    virtual std::vector<std::string> applyIR(const std::string& line,
                                             int variant_idx,
                                             const MBAConfig& config) = 0;
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_BASE_HPP
