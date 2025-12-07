/**
 * Morphect - Bogus Control Flow
 *
 * Inserts fake branches protected by opaque predicates.
 * The fake branches are never executed but complicate analysis.
 */

#ifndef MORPHECT_BOGUS_CF_HPP
#define MORPHECT_BOGUS_CF_HPP

#include "cff_base.hpp"
#include "opaque_predicates.hpp"
#include "../../core/transformation_base.hpp"

#include <string>
#include <vector>

namespace morphect {
namespace cff {

/**
 * Configuration for bogus control flow
 */
struct BogusConfig {
    bool enabled = true;
    double probability = 0.5;           // Probability of inserting bogus CF

    int min_insertions = 1;             // Min bogus branches per function
    int max_insertions = 5;             // Max bogus branches per function

    bool generate_dead_code = true;     // Generate realistic dead code
    int dead_code_lines = 3;            // Lines of dead code per bogus block

    bool use_real_variables = true;     // Use existing variables in predicates
    bool mix_predicate_types = true;    // Use different predicate types
};

/**
 * Generates fake code blocks that look realistic
 */
class DeadCodeGenerator {
public:
    /**
     * Generate dead code for LLVM IR
     * Uses provided variables to look realistic
     */
    std::vector<std::string> generateLLVM(
        const std::vector<std::string>& available_vars,
        int num_lines);

private:
    int temp_counter_ = 0;
    std::string nextTemp() {
        return "%_dead_" + std::to_string(temp_counter_++);
    }
};

/**
 * LLVM IR Bogus Control Flow Transformation
 */
class LLVMBogusControlFlow {
public:
    LLVMBogusControlFlow() : predicates_(), dead_code_gen_() {}

    /**
     * Insert bogus control flow into IR
     */
    std::vector<std::string> transform(
        const std::vector<std::string>& lines,
        const BogusConfig& config);

    /**
     * Insert a single bogus branch around real code
     */
    std::vector<std::string> insertBogusBranch(
        const std::vector<std::string>& real_code,
        const std::vector<std::string>& available_vars,
        const BogusConfig& config);

private:
    OpaquePredicateLibrary predicates_;
    DeadCodeGenerator dead_code_gen_;

    int label_counter_ = 0;
    std::string nextLabel(const std::string& prefix = "bogus") {
        return prefix + "_" + std::to_string(label_counter_++);
    }

    /**
     * Find variables available in the IR
     */
    std::vector<std::string> findVariables(const std::vector<std::string>& lines);

    /**
     * Find suitable insertion points
     */
    std::vector<size_t> findInsertionPoints(const std::vector<std::string>& lines);
};

/**
 * Bogus Control Flow Pass for LLVM IR
 */
class LLVMBogusPass : public LLVMTransformationPass {
public:
    std::string getName() const override { return "BogusControlFlow"; }
    std::string getDescription() const override {
        return "Inserts fake branches with opaque predicates";
    }

    PassPriority getPriority() const override { return PassPriority::ControlFlow; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        bogus_config_.enabled = config.enabled;
        bogus_config_.probability = config.probability;
        return true;
    }

    void setBogusConfig(const BogusConfig& config) {
        bogus_config_ = config;
    }

    TransformResult transformIR(std::vector<std::string>& lines) override;

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    LLVMBogusControlFlow transformer_;
    BogusConfig bogus_config_;
};

// ============================================================================
// Implementation
// ============================================================================

inline std::vector<std::string> DeadCodeGenerator::generateLLVM(
    const std::vector<std::string>& available_vars,
    int num_lines) {

    std::vector<std::string> code;

    for (int i = 0; i < num_lines; i++) {
        std::string t1 = nextTemp();
        std::string t2 = nextTemp();

        // Pick random operation
        int op = GlobalRandom::nextInt(0, 4);

        if (available_vars.size() >= 2) {
            // Use existing variables
            std::string v1 = available_vars[GlobalRandom::nextInt(0, static_cast<int>(available_vars.size()) - 1)];
            std::string v2 = available_vars[GlobalRandom::nextInt(0, static_cast<int>(available_vars.size()) - 1)];

            switch (op) {
                case 0:
                    code.push_back("  " + t1 + " = add i32 " + v1 + ", " + v2);
                    break;
                case 1:
                    code.push_back("  " + t1 + " = xor i32 " + v1 + ", " + v2);
                    break;
                case 2:
                    code.push_back("  " + t1 + " = and i32 " + v1 + ", " + v2);
                    break;
                case 3:
                    code.push_back("  " + t1 + " = or i32 " + v1 + ", " + v2);
                    break;
                case 4:
                    code.push_back("  " + t1 + " = mul i32 " + v1 + ", " + v2);
                    break;
            }
        } else {
            // Use constants
            int c1 = GlobalRandom::nextInt(1, 100);
            int c2 = GlobalRandom::nextInt(1, 100);

            switch (op) {
                case 0:
                    code.push_back("  " + t1 + " = add i32 " + std::to_string(c1) + ", " + std::to_string(c2));
                    break;
                case 1:
                    code.push_back("  " + t1 + " = xor i32 " + std::to_string(c1) + ", " + std::to_string(c2));
                    break;
                default:
                    code.push_back("  " + t1 + " = mul i32 " + std::to_string(c1) + ", " + std::to_string(c2));
                    break;
            }
        }
    }

    return code;
}

inline std::vector<std::string> LLVMBogusControlFlow::transform(
    const std::vector<std::string>& lines,
    const BogusConfig& config) {

    if (!config.enabled) {
        return lines;
    }

    // Find available variables
    auto vars = findVariables(lines);

    // Find insertion points (after regular instructions, before terminators)
    auto insertion_points = findInsertionPoints(lines);

    if (insertion_points.empty()) {
        return lines;
    }

    // Decide how many insertions
    int num_insertions = GlobalRandom::nextInt(
        config.min_insertions,
        std::min(config.max_insertions, static_cast<int>(insertion_points.size())));

    // Shuffle and pick insertion points
    std::vector<size_t> selected_points;
    for (size_t idx : insertion_points) {
        if (GlobalRandom::decide(config.probability)) {
            selected_points.push_back(idx);
            if (static_cast<int>(selected_points.size()) >= num_insertions) break;
        }
    }

    if (selected_points.empty()) {
        return lines;
    }

    // Sort in reverse order so we can insert without messing up indices
    std::sort(selected_points.rbegin(), selected_points.rend());

    std::vector<std::string> result = lines;

    for (size_t point : selected_points) {
        // Get the instruction at this point
        std::vector<std::string> real_code = {result[point]};

        // Generate bogus branch
        auto bogus_code = insertBogusBranch(real_code, vars, config);

        // Replace the instruction with the bogus version
        result.erase(result.begin() + point);
        result.insert(result.begin() + point, bogus_code.begin(), bogus_code.end());
    }

    return result;
}

inline std::vector<std::string> LLVMBogusControlFlow::insertBogusBranch(
    const std::vector<std::string>& real_code,
    const std::vector<std::string>& available_vars,
    const BogusConfig& config) {

    std::vector<std::string> output;

    // Get variables for predicate
    std::string var1 = available_vars.empty() ? "0" :
        available_vars[GlobalRandom::nextInt(0, static_cast<int>(available_vars.size()) - 1)];
    std::string var2 = available_vars.size() < 2 ? var1 :
        available_vars[GlobalRandom::nextInt(0, static_cast<int>(available_vars.size()) - 1)];

    // Generate opaque predicate (always true)
    auto [pred_code, pred_var] = predicates_.generateAlwaysTrue(var1, var2);

    // Labels
    std::string real_label = nextLabel("real");
    std::string fake_label = nextLabel("fake");
    std::string merge_label = nextLabel("merge");

    // Insert predicate evaluation
    for (const auto& line : pred_code) {
        output.push_back(line);
    }

    // Conditional branch (predicate is always true, so real path always taken)
    output.push_back("  br i1 " + pred_var + ", label %" + real_label + ", label %" + fake_label);
    output.push_back("");

    // Real block (always executed)
    output.push_back(real_label + ":");
    for (const auto& line : real_code) {
        output.push_back(line);
    }
    output.push_back("  br label %" + merge_label);
    output.push_back("");

    // Fake block (never executed)
    output.push_back(fake_label + ":");
    if (config.generate_dead_code) {
        auto dead_code = dead_code_gen_.generateLLVM(available_vars, config.dead_code_lines);
        for (const auto& line : dead_code) {
            output.push_back(line);
        }
    }
    output.push_back("  br label %" + merge_label);
    output.push_back("");

    // Merge block
    output.push_back(merge_label + ":");

    return output;
}

inline std::vector<std::string> LLVMBogusControlFlow::findVariables(
    const std::vector<std::string>& lines) {

    std::vector<std::string> vars;

    for (const auto& line : lines) {
        // Look for SSA assignments: %name = ...
        size_t eq_pos = line.find(" = ");
        if (eq_pos != std::string::npos) {
            size_t start = line.find('%');
            if (start != std::string::npos && start < eq_pos) {
                std::string var = line.substr(start, eq_pos - start);
                // Clean up whitespace
                size_t end = var.find_first_of(" \t");
                if (end != std::string::npos) {
                    var = var.substr(0, end);
                }
                // Only add i32 variables (simplified)
                if (line.find("i32") != std::string::npos) {
                    vars.push_back(var);
                }
            }
        }
    }

    return vars;
}

inline std::vector<size_t> LLVMBogusControlFlow::findInsertionPoints(
    const std::vector<std::string>& lines) {

    std::vector<size_t> points;

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];

        // Skip empty lines, labels, terminators
        if (line.empty()) continue;

        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start);

        // Skip labels
        if (trimmed.back() == ':') continue;

        // Skip terminators
        if (trimmed.find("ret ") == 0) continue;
        if (trimmed.find("br ") == 0) continue;
        if (trimmed.find("switch ") == 0) continue;
        if (trimmed.find("unreachable") == 0) continue;

        // Skip phi nodes (must be at start of block)
        if (trimmed.find("phi ") != std::string::npos) continue;

        // Skip function markers
        if (trimmed.find("define ") == 0) continue;
        if (trimmed.find("}") != std::string::npos) continue;

        // This is a good insertion point
        points.push_back(i);
    }

    return points;
}

inline TransformResult LLVMBogusPass::transformIR(std::vector<std::string>& lines) {
    if (!bogus_config_.enabled) {
        return TransformResult::Skipped;
    }

    if (!GlobalRandom::decide(bogus_config_.probability)) {
        return TransformResult::Skipped;
    }

    auto result = transformer_.transform(lines, bogus_config_);

    if (result.size() > lines.size()) {
        lines = std::move(result);
        incrementStat("bogus_branches_inserted");
        return TransformResult::Success;
    }

    return TransformResult::NotApplicable;
}

} // namespace cff
} // namespace morphect

#endif // MORPHECT_BOGUS_CF_HPP
