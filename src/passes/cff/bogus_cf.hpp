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
        // Find i32 VALUE variables defined BEFORE this point
        // Skip allocas (they're pointers), only include loads and arithmetic results
        std::vector<std::string> available_vars;
        for (size_t j = 0; j < point; j++) {
            const std::string& line = result[j];

            // Skip alloca - results are pointers, not values
            if (line.find(" = alloca ") != std::string::npos) continue;

            // Skip store - no result
            if (line.find("store ") != std::string::npos) continue;

            size_t eq_pos = line.find(" = ");
            if (eq_pos != std::string::npos) {
                size_t start = line.find('%');
                if (start != std::string::npos && start < eq_pos) {
                    std::string var = line.substr(start, eq_pos - start);
                    size_t end = var.find_first_of(" \t");
                    if (end != std::string::npos) {
                        var = var.substr(0, end);
                    }
                    // Only add i32 VALUE variables (from loads, arithmetic, etc.)
                    // Load pattern: %x = load i32, ptr %y
                    // Arith pattern: %x = add i32 %a, %b
                    if (line.find("load i32") != std::string::npos ||
                        line.find("add i32") != std::string::npos ||
                        line.find("sub i32") != std::string::npos ||
                        line.find("mul i32") != std::string::npos ||
                        line.find("and i32") != std::string::npos ||
                        line.find("or i32") != std::string::npos ||
                        line.find("xor i32") != std::string::npos) {
                        available_vars.push_back(var);
                    }
                }
            }
        }

        // Generate bogus branch (insert dead path before this point)
        std::vector<std::string> empty_real;  // Not used in new approach
        auto bogus_code = insertBogusBranch(empty_real, available_vars, config);

        // INSERT the bogus code BEFORE the target instruction (don't replace)
        result.insert(result.begin() + point, bogus_code.begin(), bogus_code.end());
    }

    return result;
}

inline std::vector<std::string> LLVMBogusControlFlow::insertBogusBranch(
    const std::vector<std::string>& /* real_code - not used anymore */,
    const std::vector<std::string>& /* available_vars - not used */,
    const BogusConfig& config) {

    // New approach: Insert a fake branch that's never taken
    // The real code continues normally, we just add an unreachable dead path

    std::vector<std::string> output;

    // Use constants for opaque predicate (safest approach)
    std::string const1 = std::to_string(GlobalRandom::nextInt(1, 1000));
    std::string const2 = std::to_string(GlobalRandom::nextInt(1, 1000));

    // Generate opaque predicate (always true)
    auto [pred_code, pred_var] = predicates_.generateAlwaysTrue(const1, const2);

    // Labels
    std::string continue_label = nextLabel("continue");
    std::string fake_label = nextLabel("fake");

    // Insert predicate evaluation
    for (const auto& line : pred_code) {
        output.push_back(line);
    }

    // Conditional branch (predicate is always true, so continue path always taken)
    output.push_back("  br i1 " + pred_var + ", label %" + continue_label + ", label %" + fake_label);
    output.push_back("");

    // Fake block (never executed)
    output.push_back(fake_label + ":");
    if (config.generate_dead_code) {
        std::vector<std::string> empty_vars;
        auto dead_code = dead_code_gen_.generateLLVM(empty_vars, config.dead_code_lines);
        for (const auto& line : dead_code) {
            output.push_back(line);
        }
    }
    // Dead path jumps back to continue (or we could use unreachable)
    output.push_back("  br label %" + continue_label);
    output.push_back("");

    // Continue label - real code continues here
    output.push_back(continue_label + ":");

    return output;
}

inline std::vector<std::string> LLVMBogusControlFlow::findVariables(
    const std::vector<std::string>& lines) {

    std::vector<std::string> vars;
    bool in_function = false;
    int brace_depth = 0;

    for (const auto& line : lines) {
        // Track function boundaries
        if (line.find("define ") != std::string::npos) {
            in_function = true;
            brace_depth = 0;
            for (char c : line) {
                if (c == '{') brace_depth++;
            }
            continue;
        }

        // Track brace depth
        for (char c : line) {
            if (c == '{') brace_depth++;
            if (c == '}') brace_depth--;
        }

        if (in_function && brace_depth <= 0) {
            in_function = false;
            continue;
        }

        // Only look for variables inside functions
        if (!in_function) continue;

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
    bool in_function = false;
    int brace_depth = 0;
    bool at_block_start = false;
    bool found_in_block = false;
    bool skip_entry_block = true;  // Skip first block (entry) in each function

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];

        // Track function boundaries
        if (line.find("define ") != std::string::npos) {
            in_function = true;
            brace_depth = 0;
            at_block_start = false;  // Will be set true after skipping entry allocas
            found_in_block = false;
            skip_entry_block = true;
            // Count braces on define line
            for (char c : line) {
                if (c == '{') brace_depth++;
            }
            continue;
        }

        // Track brace depth
        for (char c : line) {
            if (c == '{') brace_depth++;
            if (c == '}') brace_depth--;
        }

        // Check if we've exited the function
        if (in_function && brace_depth <= 0) {
            in_function = false;
            at_block_start = false;
            continue;
        }

        // Only consider points inside functions
        if (!in_function) continue;

        // Skip empty lines
        if (line.empty()) continue;

        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start);

        // Track basic blocks (labels) - mark that we just entered a new block
        // Labels can be numeric like "7:" or named like "entry:"
        // Format: "labelname:" possibly followed by spaces/comments
        size_t colon_pos = trimmed.find(':');
        if (colon_pos != std::string::npos && colon_pos > 0) {
            // Check if everything before the colon is alphanumeric (valid label)
            std::string potential_label = trimmed.substr(0, colon_pos);
            bool is_label = true;
            for (char c : potential_label) {
                if (!std::isalnum(c) && c != '_' && c != '.') {
                    is_label = false;
                    break;
                }
            }
            if (is_label) {
                at_block_start = true;
                found_in_block = false;
                skip_entry_block = false;  // After first label, we're past entry block
                continue;
            }
        }

        // Skip comments
        if (trimmed[0] == ';') continue;

        // Skip terminators - these end basic blocks
        if (trimmed.find("ret ") == 0 || trimmed.find("ret void") == 0 ||
            trimmed.find("br ") == 0 || trimmed.find("switch ") == 0 ||
            trimmed.find("unreachable") == 0 || trimmed.find("invoke ") == 0) {
            at_block_start = false;
            found_in_block = false;
            continue;
        }

        // Skip phi nodes (must be at start of block)
        if (trimmed.find(" phi ") != std::string::npos) continue;

        // Skip alloca (should be at function entry)
        if (trimmed.find(" = alloca ") != std::string::npos) continue;

        // Skip entry block - bogus flow there could break function prologue
        if (skip_entry_block) continue;

        // Find first suitable instruction after block label
        if (at_block_start && !found_in_block) {
            // Skip stores at the very start of blocks (common pattern)
            if (trimmed.find("store ") == 0) continue;

            // This is a good insertion point
            found_in_block = true;
            at_block_start = false;
            points.push_back(i);
        }
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
