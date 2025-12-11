/**
 * Morphect - Dead Code Insertion for LLVM IR
 *
 * Inserts realistic-looking dead code that never executes,
 * guarded by opaque predicates.
 *
 * Dead Code Types:
 *   - Arithmetic: Operations that compute unused results
 *   - Memory: Load/store to stack variables
 *   - Calls: Function calls to nop functions
 *   - MBA: MBA-obfuscated arithmetic (looks like real obfuscation)
 *
 * Example output:
 *   ; Opaque predicate setup (always false)
 *   %_opq_v = add i32 0, 42
 *   %_opq_sq = mul i32 %_opq_v, %_opq_v
 *   %_opq_cond = icmp slt i32 %_opq_sq, 0
 *   br i1 %_opq_cond, label %dead_block0, label %continue0
 *   dead_block0:
 *     ; Dead code (never executed)
 *     %_dead0 = add i32 %real_var, 100
 *     %_dead1 = mul i32 %_dead0, 7
 *     call void @_validate_1234(i32 %_dead1)
 *     br label %continue0
 *   continue0:
 *     ; Real code continues here
 */

#ifndef MORPHECT_DEAD_CODE_HPP
#define MORPHECT_DEAD_CODE_HPP

#include "dead_code_base.hpp"

#include <regex>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace morphect {
namespace deadcode {

/**
 * LLVM IR Code Analyzer for dead code insertion
 */
class LLVMCodeAnalyzer {
public:
    /**
     * Check if a function is CFF-flattened (uses dispatcher pattern)
     */
    bool isCFFFlattened(const std::vector<std::string>& lines,
                        size_t start_line, size_t end_line) const {
        if (end_line > lines.size()) end_line = lines.size();
        for (size_t i = start_line; i < end_line; i++) {
            const auto& line = lines[i];
            // CFF-flattened functions have dispatcher pattern
            if (line.find("dispatcher:") != std::string::npos ||
                line.find("%_cff_state") != std::string::npos ||
                line.find("entry_flat:") != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    /**
     * Extract variable information from IR (within a specific function scope)
     *
     * @param lines All IR lines
     * @param start_line Starting line of the function (or 0 for all)
     * @param end_line Ending line of the function (or lines.size() for all)
     * @param only_allocas If true, only extract alloca variables (for CFF-flattened code)
     */
    std::vector<VariableInfo> extractVariables(
        const std::vector<std::string>& lines,
        size_t start_line = 0,
        size_t end_line = SIZE_MAX,
        bool only_allocas = false) const {

        std::vector<VariableInfo> vars;
        std::unordered_set<std::string> seen;

        // Pattern: %name = ...
        std::regex assign_re(R"(%([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
        // Pattern: i32 %name or i64 %name etc.
        std::regex typed_var_re(R"((i\d+|float|double)\s+%([a-zA-Z_][a-zA-Z0-9_]*))");

        if (end_line > lines.size()) end_line = lines.size();

        for (size_t i = start_line; i < end_line; i++) {
            const auto& line = lines[i];
            // Skip comments and empty lines
            if (line.empty() || line[0] == ';') continue;

            std::smatch match;

            // Find assignments
            if (std::regex_search(line, match, assign_re)) {
                std::string name = "%" + match[1].str();
                if (seen.find(name) == seen.end()) {
                    bool is_alloca = (line.find("alloca") != std::string::npos);

                    // In CFF-flattened code, only use allocas (they dominate all states)
                    // Skip non-alloca variables as they may be defined in different states
                    if (only_allocas && !is_alloca) {
                        continue;
                    }

                    seen.insert(name);

                    VariableInfo var;
                    var.name = name;
                    var.type = inferType(line);
                    var.is_pointer = is_alloca || (line.find("*") != std::string::npos);
                    vars.push_back(var);
                }
            }

            // Only extract typed variables in operands if not in only_allocas mode
            // These are SSA values that may not dominate the insertion point
            if (!only_allocas) {
                std::string::const_iterator searchStart(line.cbegin());
                while (std::regex_search(searchStart, line.cend(), match, typed_var_re)) {
                    std::string name = "%" + match[2].str();
                    if (seen.find(name) == seen.end()) {
                        seen.insert(name);

                        VariableInfo var;
                        var.name = name;
                        var.type = match[1].str();
                        vars.push_back(var);
                    }
                    searchStart = match.suffix().first;
                }
            }
        }

        return vars;
    }

    /**
     * Find function boundaries (start and end line for each function)
     * Returns map of function_start_line -> (function_start, function_end)
     */
    std::vector<std::pair<size_t, size_t>> findFunctionBoundaries(
        const std::vector<std::string>& lines) const {

        std::vector<std::pair<size_t, size_t>> boundaries;
        size_t func_start = 0;
        bool in_function = false;

        for (size_t i = 0; i < lines.size(); i++) {
            const std::string& line = lines[i];

            if (line.find("define ") != std::string::npos) {
                in_function = true;
                func_start = i;
                continue;
            }
            if (line == "}" && in_function) {
                boundaries.push_back({func_start, i});
                in_function = false;
            }
        }

        return boundaries;
    }

    /**
     * Get function boundary containing the given line
     */
    std::pair<size_t, size_t> getFunctionBoundary(
        const std::vector<std::pair<size_t, size_t>>& boundaries,
        size_t line_num) const {

        for (const auto& bound : boundaries) {
            if (line_num >= bound.first && line_num <= bound.second) {
                return bound;
            }
        }
        return {0, SIZE_MAX};  // Fallback
    }

    /**
     * Find suitable insertion points for dead code
     * Returns line indices where dead code can be inserted
     */
    std::vector<int> findInsertionPoints(
        const std::vector<std::string>& lines) const {

        std::vector<int> points;
        bool in_function = false;
        bool after_label = false;

        for (size_t i = 0; i < lines.size(); i++) {
            const std::string& line = lines[i];

            // Track function boundaries
            if (line.find("define ") != std::string::npos) {
                in_function = true;
                continue;
            }
            if (line == "}") {
                in_function = false;
                continue;
            }

            if (!in_function) continue;

            // Track labels
            if (line.find(':') != std::string::npos &&
                line.find("label") == std::string::npos &&
                !line.empty() && line[0] != ' ') {
                after_label = true;
                continue;
            }

            // Good insertion points: after a label, before certain instructions
            if (after_label) {
                // Insert after first non-phi instruction following a label
                if (line.find("phi ") == std::string::npos) {
                    points.push_back(static_cast<int>(i));
                    after_label = false;
                }
            }

            // Also insert before branch/ret (but not conditional branch to avoid issues)
            if ((line.find("  br label") != std::string::npos ||
                 line.find("  ret ") != std::string::npos) &&
                line.find("br i1") == std::string::npos) {
                points.push_back(static_cast<int>(i));
            }
        }

        return points;
    }

    /**
     * Extract function names from IR
     */
    std::vector<std::string> extractFunctionNames(
        const std::vector<std::string>& lines) const {

        std::vector<std::string> funcs;
        std::regex func_re(R"(define\s+(?:internal\s+)?(?:void|i\d+|float|double)\s+@([a-zA-Z_][a-zA-Z0-9_]*))");

        for (const auto& line : lines) {
            std::smatch match;
            if (std::regex_search(line, match, func_re)) {
                funcs.push_back(match[1].str());
            }
        }

        return funcs;
    }

private:
    std::string inferType(const std::string& line) const {
        if (line.find("i64") != std::string::npos) return "i64";
        if (line.find("i32") != std::string::npos) return "i32";
        if (line.find("i16") != std::string::npos) return "i16";
        if (line.find("i8") != std::string::npos) return "i8";
        if (line.find("i1") != std::string::npos) return "i1";
        if (line.find("double") != std::string::npos) return "double";
        if (line.find("float") != std::string::npos) return "float";
        return "i32";  // Default
    }
};

/**
 * LLVM IR Dead Code Transformation
 */
class LLVMDeadCodeTransformation : public DeadCodeTransformation {
public:
    std::string getName() const override { return "LLVM_DeadCode"; }

    DeadCodeResult transform(
        const std::vector<std::string>& lines,
        const DeadCodeConfig& config) override {

        DeadCodeResult result;
        result.transformed_code = lines;

        if (!config.enabled) {
            result.success = true;
            return result;
        }

        // Analyze the code
        LLVMCodeAnalyzer analyzer;
        auto function_boundaries = analyzer.findFunctionBoundaries(lines);
        auto insertion_points = analyzer.findInsertionPoints(lines);
        auto function_names = analyzer.extractFunctionNames(lines);

        if (insertion_points.empty()) {
            result.success = true;
            return result;
        }

        // Create generators
        std::vector<std::unique_ptr<DeadCodeGenerator>> generators;
        generators.push_back(std::make_unique<DeadArithmeticGenerator>());
        generators.push_back(std::make_unique<DeadMemoryGenerator>());
        generators.push_back(std::make_unique<DeadCallGenerator>());
        if (config.apply_mba) {
            generators.push_back(std::make_unique<MBADeadCodeGenerator>());
        }

        // Select random insertion points
        // nextInt is INCLUSIVE, so don't add 1 to max
        int num_insertions = GlobalRandom::nextInt(config.min_blocks,
            std::min(config.max_blocks,
                    static_cast<int>(insertion_points.size())));

        // Shuffle and select
        std::vector<int> selected_points = insertion_points;
        for (size_t i = selected_points.size() - 1; i > 0; i--) {
            // nextInt is INCLUSIVE, so i is the max valid index
            size_t j = static_cast<size_t>(GlobalRandom::nextInt(0, static_cast<int>(i)));
            std::swap(selected_points[i], selected_points[j]);
        }

        // Limit to max insertions
        if (static_cast<int>(selected_points.size()) > num_insertions) {
            selected_points.resize(num_insertions);
        }

        // Sort in reverse order for safe insertion
        std::sort(selected_points.rbegin(), selected_points.rend());

        // Track nop functions to create
        std::vector<std::pair<std::string, int>> nop_functions;  // name, num_args

        // Insert dead code at selected points
        for (int insert_point : selected_points) {
            if (GlobalRandom::nextDouble() > config.probability) {
                continue;
            }

            // Extract variables only from the function containing this insertion point
            // This ensures dead code doesn't use variables from other functions
            auto func_bounds = analyzer.getFunctionBoundary(function_boundaries,
                                                            static_cast<size_t>(insert_point));

            // IMPORTANT: We don't use real variables in dead code generation.
            // Using existing SSA values can violate SSA domination rules since we don't
            // have full dominator analysis. A variable defined in one branch may not
            // dominate the insertion point. Safe approach: use constants only.
            //
            // This is especially critical for:
            // 1. CFF-flattened code (states don't dominate each other)
            // 2. Branches in normal CFG (variables in if/else don't dominate later code)
            // 3. Variable splitting (splits create new SSA values in their local scope)
            std::vector<VariableInfo> variables;  // Empty = generators use constants only

            // Select a generator based on weights
            double r = GlobalRandom::nextDouble();
            DeadCodeGenerator* gen = nullptr;

            if (r < config.arithmetic_probability) {
                gen = generators[0].get();
            } else if (r < config.arithmetic_probability + config.memory_probability) {
                gen = generators[1].get();
            } else if (r < config.arithmetic_probability + config.memory_probability +
                       config.call_probability) {
                gen = generators[2].get();
            } else if (config.apply_mba && generators.size() > 3) {
                gen = generators[3].get();
            } else {
                gen = generators[0].get();
            }

            // Generate dead code block
            DeadCodeBlock block = gen->generate(variables, config);

            // Track nop functions
            for (const auto& func : block.nop_functions_created) {
                int num_args = GlobalRandom::nextInt(0, 3);
                nop_functions.push_back(std::make_pair(func, num_args));
            }

            // Generate guarded dead code
            auto guarded_code = wrapWithOpaqueGuard(block, config);

            // Insert into code
            result.transformed_code.insert(
                result.transformed_code.begin() + insert_point,
                guarded_code.begin(), guarded_code.end());

            result.blocks_inserted++;
            result.ops_inserted += static_cast<int>(block.code.size());
            result.calls_inserted += block.calls_inserted;
            result.memory_ops_inserted += block.memory_ops_inserted;
        }

        // Insert nop function definitions at the start (before first function)
        if (!nop_functions.empty()) {
            insertNopFunctions(result.transformed_code, nop_functions);
            for (const auto& [name, _] : nop_functions) {
                result.nop_functions_created.push_back(name);
            }
        }

        result.success = true;
        return result;
    }

private:
    int label_counter_ = 0;

    /**
     * Wrap dead code with opaque predicate guard
     */
    std::vector<std::string> wrapWithOpaqueGuard(
        const DeadCodeBlock& block,
        const DeadCodeConfig& config) {

        std::vector<std::string> result;
        std::string prefix = "_dead" + std::to_string(GlobalRandom::nextInt(1000, 9999));
        std::string dead_label = "dead_block" + std::to_string(label_counter_);
        std::string cont_label = "continue" + std::to_string(label_counter_);
        label_counter_++;

        // Generate opaque predicate (always false)
        auto [condition, setup] = OpaquePredicateGen::generateAlwaysFalse(prefix);

        result.push_back("  ; Dead code block (never executed)");

        // Add setup code
        for (const auto& line : setup) {
            result.push_back(line);
        }

        // Add condition and branch
        std::string cond_var = "%" + prefix + "_cond";
        result.push_back("  " + cond_var + " = " + condition);
        result.push_back("  br i1 " + cond_var + ", label %" + dead_label +
                        ", label %" + cont_label);

        // Dead block
        result.push_back(dead_label + ":");
        for (const auto& line : block.code) {
            result.push_back(line);
        }
        result.push_back("  br label %" + cont_label);

        // Continue block
        result.push_back(cont_label + ":");

        return result;
    }

    /**
     * Insert nop function definitions before first function
     */
    void insertNopFunctions(std::vector<std::string>& code,
                           const std::vector<std::pair<std::string, int>>& functions) {
        // Find first function definition
        size_t insert_pos = 0;
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].find("define ") != std::string::npos &&
                code[i].find("define internal") == std::string::npos) {
                insert_pos = i;
                break;
            }
        }

        // Generate and insert nop functions
        std::vector<std::string> nop_defs;
        for (const auto& [name, num_args] : functions) {
            auto def = DeadCallGenerator::generateNopFunction(name, num_args);
            nop_defs.insert(nop_defs.end(), def.begin(), def.end());
            nop_defs.push_back("");
        }

        code.insert(code.begin() + insert_pos, nop_defs.begin(), nop_defs.end());
    }
};

/**
 * LLVM IR Dead Code Insertion Pass
 */
class LLVMDeadCodePass : public LLVMTransformationPass {
public:
    LLVMDeadCodePass() : transformer_() {}

    std::string getName() const override { return "DeadCode"; }
    std::string getDescription() const override {
        return "Inserts realistic dead code guarded by opaque predicates";
    }

    PassPriority getPriority() const override { return PassPriority::Late; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        dc_config_.enabled = config.enabled;
        dc_config_.probability = config.probability;
        return true;
    }

    void setDeadCodeConfig(const DeadCodeConfig& config) {
        dc_config_ = config;
    }

    const DeadCodeConfig& getDeadCodeConfig() const {
        return dc_config_;
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!dc_config_.enabled) {
            return TransformResult::Skipped;
        }

        auto dc_result = transformer_.transform(lines, dc_config_);

        if (dc_result.success) {
            lines = std::move(dc_result.transformed_code);
            statistics_["blocks_inserted"] = dc_result.blocks_inserted;
            statistics_["ops_inserted"] = dc_result.ops_inserted;
            statistics_["calls_inserted"] = dc_result.calls_inserted;
            statistics_["memory_ops_inserted"] = dc_result.memory_ops_inserted;
            statistics_["nop_functions"] =
                static_cast<int>(dc_result.nop_functions_created.size());
            return TransformResult::Success;
        } else {
            return TransformResult::Error;
        }
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    LLVMDeadCodeTransformation transformer_;
    DeadCodeConfig dc_config_;
};

} // namespace deadcode
} // namespace morphect

#endif // MORPHECT_DEAD_CODE_HPP
