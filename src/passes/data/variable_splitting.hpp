/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * variable_splitting.hpp - Variable splitting obfuscation
 *
 * Variable splitting transforms a single variable into multiple parts:
 *   x -> x1 + x2  (additive split)
 *   x -> x1 ^ x2  (XOR split)
 *   x -> x1 * x2  (multiplicative split, for non-zero values)
 *
 * All uses of x are replaced with the reconstruction expression,
 * and all assignments split the value across the parts.
 */

#ifndef MORPHECT_VARIABLE_SPLITTING_HPP
#define MORPHECT_VARIABLE_SPLITTING_HPP

#include "data_base.hpp"
#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>

namespace morphect {
namespace data {

/**
 * Split strategy for variables
 */
enum class SplitStrategy {
    Additive,       // x = x1 + x2
    XOR,            // x = x1 ^ x2
    Multiplicative  // x = x1 * x2 (only for known non-zero)
};

/**
 * Information about a split variable
 */
struct SplitVariable {
    std::string original_name;   // Original variable name (e.g., %x)
    std::string type;            // LLVM type (e.g., i32)
    std::string part1_name;      // First part name (e.g., %x_split1)
    std::string part2_name;      // Second part name (e.g., %x_split2)
    SplitStrategy strategy;      // How the variable is split
    bool is_phi_node = false;    // Whether this is a PHI node
    int num_parts = 2;           // Number of parts (2 for now)
};

/**
 * Configuration for variable splitting
 */
struct VariableSplittingConfig {
    bool enabled = true;
    double probability = 0.5;           // Probability to split a variable
    SplitStrategy default_strategy = SplitStrategy::Additive;
    bool split_phi_nodes = true;        // Whether to split PHI nodes
    int max_splits_per_function = 10;   // Limit splits to prevent explosion
    std::vector<std::string> exclude_patterns;  // Variable patterns to exclude
    bool prefer_xor_for_loops = true;   // Use XOR for loop counters
};

/**
 * Result of analyzing a variable for splitting
 */
struct VariableAnalysis {
    std::string name;
    std::string type;
    std::vector<int> definition_lines;  // Lines where variable is defined
    std::vector<int> use_lines;         // Lines where variable is used
    bool is_phi = false;
    bool is_loop_counter = false;
    bool can_split = true;
    std::string reason_cannot_split;
};

/**
 * LLVM IR Variable Splitting Pass
 *
 * Transforms LLVM IR to split selected variables into multiple parts.
 */
class LLVMVariableSplittingPass : public LLVMTransformationPass {
public:
    LLVMVariableSplittingPass() : logger_("VarSplit") {}

    std::string getName() const override { return "VariableSplitting"; }
    std::string getDescription() const override {
        return "Split variables into multiple parts for obfuscation";
    }

    PassPriority getPriority() const override { return PassPriority::Data; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        config_.probability = config.probability;
        config_.enabled = config.enabled;
        return true;
    }

    /**
     * Configure with variable splitting specific options
     */
    void configure(const VariableSplittingConfig& config) {
        config_ = config;
    }

    /**
     * Transform LLVM IR lines
     */
    TransformResult transformIR(std::vector<std::string>& lines) override;

    /**
     * Get statistics
     */
    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    mutable Logger logger_;
    VariableSplittingConfig config_;

    // Track split variables
    std::map<std::string, SplitVariable> split_vars_;

    // Counter for unique names
    int split_counter_ = 0;

    /**
     * Analyze a function's IR to find splittable variables
     */
    std::vector<VariableAnalysis> analyzeVariables(
        const std::vector<std::string>& lines,
        size_t func_start,
        size_t func_end);

    /**
     * Select which variables to split
     */
    std::vector<std::string> selectVariablesToSplit(
        const std::vector<VariableAnalysis>& analyses);

    /**
     * Create split variable info for a variable
     */
    SplitVariable createSplitVariable(
        const std::string& name,
        const std::string& type);

    /**
     * Transform a single function's IR with variable splitting
     */
    std::vector<std::string> transformFunction(
        const std::vector<std::string>& lines,
        size_t func_start,
        size_t func_end);

    /**
     * Transform a definition (assignment) of a split variable
     * e.g., %x = add i32 %a, %b
     * becomes:
     *   %x_split1 = ... (random part)
     *   %x_split2 = sub i32 %result, %x_split1  (remainder)
     */
    std::vector<std::string> transformDefinition(
        const std::string& line,
        const SplitVariable& split_var);

    /**
     * Transform a use of a split variable
     * e.g., %y = add i32 %x, 1
     * becomes:
     *   %x_reconst = add i32 %x_split1, %x_split2
     *   %y = add i32 %x_reconst, 1
     */
    std::vector<std::string> transformUse(
        const std::string& line,
        const std::map<std::string, SplitVariable>& active_splits);

    /**
     * Transform a PHI node with split variables
     */
    std::vector<std::string> transformPhiNode(
        const std::string& line,
        const SplitVariable& split_var);

    /**
     * Generate reconstruction expression for a split variable
     */
    std::string generateReconstruction(
        const SplitVariable& split_var,
        const std::string& result_name);

    /**
     * Generate split assignment for a value
     */
    std::vector<std::string> generateSplitAssignment(
        const std::string& value,
        const std::string& type,
        const SplitVariable& split_var);

    /**
     * Check if a variable name matches exclusion patterns
     */
    bool isExcluded(const std::string& name) const;

    /**
     * Extract variable name from an IR line
     */
    std::string extractDestVariable(const std::string& line) const;

    /**
     * Extract type from an IR assignment
     */
    std::string extractType(const std::string& line) const;

    /**
     * Find all variable uses in a line
     */
    std::vector<std::string> findVariableUses(const std::string& line) const;
};

} // namespace data
} // namespace morphect

#endif // MORPHECT_VARIABLE_SPLITTING_HPP
