/**
 * Morphect - Control Flow Obfuscation
 *
 * Main include file for control flow obfuscation passes.
 *
 * Includes:
 * - Control Flow Flattening (CFF)
 * - Bogus Control Flow (BCF)
 * - Opaque Predicates
 */

#ifndef MORPHECT_CFF_HPP
#define MORPHECT_CFF_HPP

#include "cff_base.hpp"
#include "opaque_predicates.hpp"
#include "bogus_cf.hpp"
#include "../../common/json_parser.hpp"

namespace morphect {
namespace cff {

/**
 * Unified configuration for all control flow obfuscation
 */
struct ControlFlowConfig {
    // Control Flow Flattening
    CFFConfig cff;

    // Bogus Control Flow
    BogusConfig bogus;

    // General settings
    bool enabled = true;
    double global_probability = 0.85;

    /**
     * Load from JSON configuration
     */
    void loadFromJson(const JsonValue& json) {
        if (json.has("enabled")) {
            enabled = json["enabled"].asBool(true);
        }
        if (json.has("probability")) {
            global_probability = json["probability"].asDouble(0.85);
        }

        if (json.has("cff")) {
            const auto& cff_json = json["cff"];
            cff.enabled = cff_json.has("enabled") ?
                cff_json["enabled"].asBool(true) : true;
            cff.probability = cff_json.has("probability") ?
                cff_json["probability"].asDouble(0.85) : global_probability;
            cff.min_blocks = cff_json.has("min_blocks") ?
                cff_json["min_blocks"].asInt(3) : 3;
            cff.max_blocks = cff_json.has("max_blocks") ?
                cff_json["max_blocks"].asInt(100) : 100;
            cff.shuffle_states = cff_json.has("shuffle_states") ?
                cff_json["shuffle_states"].asBool(true) : true;
        }

        if (json.has("bogus")) {
            const auto& bogus_json = json["bogus"];
            bogus.enabled = bogus_json.has("enabled") ?
                bogus_json["enabled"].asBool(true) : true;
            bogus.probability = bogus_json.has("probability") ?
                bogus_json["probability"].asDouble(0.5) : 0.5;
            bogus.min_insertions = bogus_json.has("min_insertions") ?
                bogus_json["min_insertions"].asInt(1) : 1;
            bogus.max_insertions = bogus_json.has("max_insertions") ?
                bogus_json["max_insertions"].asInt(5) : 5;
            bogus.generate_dead_code = bogus_json.has("generate_dead_code") ?
                bogus_json["generate_dead_code"].asBool(true) : true;
        }
    }
};

/**
 * Combined Control Flow Pass for LLVM IR
 *
 * Applies both CFF and Bogus CF transformations.
 */
class LLVMControlFlowPass : public LLVMTransformationPass {
public:
    LLVMControlFlowPass() : cff_pass_(), bogus_pass_() {}

    std::string getName() const override { return "ControlFlow"; }
    std::string getDescription() const override {
        return "Combined control flow obfuscation (CFF + Bogus)";
    }

    PassPriority getPriority() const override { return PassPriority::ControlFlow; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        cf_config_.enabled = config.enabled;
        cf_config_.global_probability = config.probability;

        // Initialize sub-passes
        cff_pass_.setCFFConfig(cf_config_.cff);
        bogus_pass_.setBogusConfig(cf_config_.bogus);

        return true;
    }

    void setControlFlowConfig(const ControlFlowConfig& config) {
        cf_config_ = config;
        cff_pass_.setCFFConfig(config.cff);
        bogus_pass_.setBogusConfig(config.bogus);
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!cf_config_.enabled) {
            return TransformResult::Skipped;
        }

        bool transformed = false;

        // Apply CFF first (major structural change)
        if (cf_config_.cff.enabled) {
            auto result = cff_pass_.transformIR(lines);
            if (result == TransformResult::Success) {
                transformed = true;
                auto cff_stats = cff_pass_.getStatistics();
                for (const auto& [key, value] : cff_stats) {
                    incrementStat("cff_" + key, value);
                }
            }
        }

        // Then apply Bogus CF
        if (cf_config_.bogus.enabled) {
            auto result = bogus_pass_.transformIR(lines);
            if (result == TransformResult::Success) {
                transformed = true;
                auto bogus_stats = bogus_pass_.getStatistics();
                for (const auto& [key, value] : bogus_stats) {
                    incrementStat("bogus_" + key, value);
                }
            }
        }

        return transformed ? TransformResult::Success : TransformResult::NotApplicable;
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    ControlFlowConfig cf_config_;
    LLVMCFFPass cff_pass_;
    LLVMBogusPass bogus_pass_;
};

} // namespace cff
} // namespace morphect

#endif // MORPHECT_CFF_HPP
