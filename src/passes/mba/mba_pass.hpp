/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba_pass.hpp - Unified MBA (Mixed Boolean Arithmetic) Pass
 *
 * This is the main entry point for all MBA transformations.
 * It coordinates ADD, SUB, XOR, AND, OR, and MULT transformations.
 */

#ifndef MORPHECT_MBA_PASS_HPP
#define MORPHECT_MBA_PASS_HPP

#include "mba_base.hpp"
#include "mba_add.hpp"
#include "mba_sub.hpp"
#include "mba_xor.hpp"
#include "mba_and.hpp"
#include "mba_or.hpp"
#include "mba_mult.hpp"

#include "../../core/transformation_base.hpp"
#include "../../core/statistics.hpp"
#include "../../common/json_parser.hpp"

#include <memory>
#include <unordered_map>

namespace morphect {
namespace mba {

/**
 * Configuration for the MBA pass
 */
struct MBAPassConfig {
    MBAConfig global;           // Global MBA settings

    // Per-operation enable flags
    bool enable_add = true;
    bool enable_sub = true;
    bool enable_xor = true;
    bool enable_and = true;
    bool enable_or = true;
    bool enable_mult = true;

    // Per-operation configs (if different from global)
    std::unordered_map<std::string, MBAConfig> operation_configs;

    /**
     * Get config for a specific operation
     */
    const MBAConfig& getConfig(const std::string& op) const {
        auto it = operation_configs.find(op);
        if (it != operation_configs.end()) {
            return it->second;
        }
        return global;
    }

    /**
     * Load configuration from JSON
     */
    void loadFromJson(const JsonValue& json) {
        if (json.has("global_probability")) {
            global.probability = json["global_probability"].asDouble(0.85);
        }
        if (json.has("nesting_depth")) {
            global.nesting_depth = json["nesting_depth"].asInt(1);
        }
        if (json.has("use_all_variants")) {
            global.use_all_variants = json["use_all_variants"].asBool(false);
        }

        // Per-operation settings
        if (json.has("mba_transformations")) {
            const auto& mba = json["mba_transformations"];

            if (mba.has("add")) {
                enable_add = mba["add"].has("enabled") ?
                    mba["add"]["enabled"].asBool(true) : true;
            }
            if (mba.has("sub")) {
                enable_sub = mba["sub"].has("enabled") ?
                    mba["sub"]["enabled"].asBool(true) : true;
            }
            if (mba.has("xor")) {
                enable_xor = mba["xor"].has("enabled") ?
                    mba["xor"]["enabled"].asBool(true) : true;
            }
            if (mba.has("and")) {
                enable_and = mba["and"].has("enabled") ?
                    mba["and"]["enabled"].asBool(true) : true;
            }
            if (mba.has("or")) {
                enable_or = mba["or"].has("enabled") ?
                    mba["or"]["enabled"].asBool(true) : true;
            }
            if (mba.has("mult")) {
                enable_mult = mba["mult"].has("enabled") ?
                    mba["mult"]["enabled"].asBool(true) : true;  // Enabled by default
            }
        }
    }
};

/**
 * LLVM IR MBA Pass
 *
 * Transforms LLVM IR text by applying MBA transformations
 * to arithmetic and bitwise operations.
 */
class LLVMMBAPass : public LLVMTransformationPass {
public:
    LLVMMBAPass() : logger_("MBA_Pass") {
        // Initialize transformations
        transforms_["add"] = std::make_unique<LLVMMBAAdd>();
        transforms_["sub"] = std::make_unique<LLVMMBASub>();
        transforms_["xor"] = std::make_unique<LLVMMBAXor>();
        transforms_["and"] = std::make_unique<LLVMMBAAnd>();
        transforms_["or"] = std::make_unique<LLVMMBAOr>();
        transforms_["mul"] = std::make_unique<LLVMMBAMult>();
    }

    std::string getName() const override { return "MBA"; }
    std::string getDescription() const override {
        return "Mixed Boolean Arithmetic transformations";
    }

    PassPriority getPriority() const override { return PassPriority::MBA; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        pass_config_.global.probability = config.probability;
        pass_config_.global.enabled = config.enabled;
        return true;
    }

    /**
     * Initialize with MBA-specific config
     */
    void initializeMBA(const MBAPassConfig& config) {
        pass_config_ = config;
    }

    /**
     * Transform LLVM IR lines
     */
    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!pass_config_.global.enabled) {
            return TransformResult::Skipped;
        }

        int total_transformations = 0;

        // Apply transformations with nesting
        int nesting_depth = pass_config_.global.nesting_depth;
        if (nesting_depth < 1) nesting_depth = 1;
        if (nesting_depth > 5) nesting_depth = 5;  // Cap at 5 to prevent explosion

        for (int depth = 0; depth < nesting_depth; depth++) {
            auto [new_lines, transformations] = applyTransformationPass(lines, depth);
            lines = std::move(new_lines);
            total_transformations += transformations;

            // If no transformations at this depth, no point continuing
            if (transformations == 0) {
                break;
            }

            logger_.debug("Nesting depth {}: {} transformations", depth, transformations);
        }

        // Renumber SSA values to fix sequential numbering after transformations
        if (total_transformations > 0) {
            renumberSSA(lines);
        }

        incrementStat("total_transformations", total_transformations);
        incrementStat("nesting_depth_used", nesting_depth);

        return total_transformations > 0 ? TransformResult::Success : TransformResult::NotApplicable;
    }

private:
    /**
     * Apply one pass of MBA transformations
     * Returns the transformed lines and count of transformations
     */
    std::pair<std::vector<std::string>, int> applyTransformationPass(
            const std::vector<std::string>& lines, int current_depth) {

        std::vector<std::string> new_lines;
        new_lines.reserve(lines.size() * 2);  // Estimate expansion
        int transformations = 0;

        // For nested passes, reduce probability to avoid explosion
        MBAConfig nested_config = pass_config_.global;
        if (current_depth > 0) {
            // Reduce probability for deeper nesting to limit explosion
            // Depth 1: 70%, Depth 2: 50%, Depth 3: 30%, etc.
            double reduction = 1.0 - (current_depth * 0.2);
            if (reduction < 0.2) reduction = 0.2;
            nested_config.probability = pass_config_.global.probability * reduction;
        }

        for (const auto& line : lines) {
            auto [result, op_name] = tryTransformLine(line, nested_config);

            if (!result.empty()) {
                for (const auto& r : result) {
                    new_lines.push_back(r);
                }
                transformations++;
                incrementStat(op_name + "_applied");
            } else {
                new_lines.push_back(line);
            }
        }

        return {new_lines, transformations};
    }

    /**
     * Try to transform a single line with any applicable transformation
     * Returns the result lines and the operation name that was applied
     */
    std::pair<std::vector<std::string>, std::string> tryTransformLine(
            const std::string& line, const MBAConfig& config) {

        // Try each transformation in order
        if (pass_config_.enable_add) {
            auto result = transforms_["add"]->applyIR(line, -1, config);
            if (!result.empty()) {
                return {result, "add"};
            }
        }

        if (pass_config_.enable_sub) {
            auto result = transforms_["sub"]->applyIR(line, -1, config);
            if (!result.empty()) {
                return {result, "sub"};
            }
        }

        if (pass_config_.enable_xor) {
            auto result = transforms_["xor"]->applyIR(line, -1, config);
            if (!result.empty()) {
                return {result, "xor"};
            }
        }

        if (pass_config_.enable_and) {
            auto result = transforms_["and"]->applyIR(line, -1, config);
            if (!result.empty()) {
                return {result, "and"};
            }
        }

        if (pass_config_.enable_or) {
            auto result = transforms_["or"]->applyIR(line, -1, config);
            if (!result.empty()) {
                return {result, "or"};
            }
        }

        if (pass_config_.enable_mult) {
            auto result = transforms_["mul"]->applyIR(line, -1, config);
            if (!result.empty()) {
                return {result, "mul"};
            }
        }

        return {{}, ""};
    }

public:

    /**
     * Get statistics
     */
    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

    /**
     * Print statistics summary
     */
    void printStatistics() {
        logger_.info("=== MBA Pass Statistics ===");
        logger_.info("ADD transformations: {}", statistics_.count("add_applied") ?
            statistics_.at("add_applied") : 0);
        logger_.info("SUB transformations: {}", statistics_.count("sub_applied") ?
            statistics_.at("sub_applied") : 0);
        logger_.info("XOR transformations: {}", statistics_.count("xor_applied") ?
            statistics_.at("xor_applied") : 0);
        logger_.info("AND transformations: {}", statistics_.count("and_applied") ?
            statistics_.at("and_applied") : 0);
        logger_.info("OR transformations: {}", statistics_.count("or_applied") ?
            statistics_.at("or_applied") : 0);
        logger_.info("MULT transformations: {}", statistics_.count("mult_applied") ?
            statistics_.at("mult_applied") : 0);
        logger_.info("Total: {}", statistics_.count("total_transformations") ?
            statistics_.at("total_transformations") : 0);
        logger_.info("===========================");
    }

private:
    mutable Logger logger_;
    MBAPassConfig pass_config_;
    std::unordered_map<std::string, std::unique_ptr<LLVMMBATransformation>> transforms_;
};

} // namespace mba
} // namespace morphect

#endif // MORPHECT_MBA_PASS_HPP
