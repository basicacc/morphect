/**
 * Morphect - Control Flow Flattening Base
 *
 * Base definitions for control flow flattening transformations.
 * CFF converts a function's control flow into a switch-based state machine.
 */

#ifndef MORPHECT_CFF_BASE_HPP
#define MORPHECT_CFF_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>

namespace morphect {
namespace cff {

/**
 * Represents a basic block in the control flow graph
 */
struct BasicBlockInfo {
    int id;                          // Unique ID for this block
    std::string label;               // Block label (for IR)
    std::vector<std::string> code;   // Instructions in this block

    // Control flow
    std::vector<int> successors;     // IDs of successor blocks
    std::vector<int> predecessors;   // IDs of predecessor blocks

    // Block type
    bool is_entry = false;           // Entry block
    bool is_exit = false;            // Return/exit block
    bool is_loop_header = false;     // Loop header
    bool is_loop_latch = false;      // Loop back-edge source
    bool has_conditional = false;    // Has conditional branch
    bool has_switch = false;         // Has switch statement

    // Exception handling
    bool has_invoke = false;         // Has invoke instruction (can throw)
    bool is_landing_pad = false;     // Is a landing pad block
    bool has_resume = false;         // Has resume instruction
    int normal_dest = -1;            // Normal destination for invoke
    int unwind_dest = -1;            // Exception destination for invoke
    std::string invoke_instruction;  // The invoke instruction text

    // State machine
    int state_value = -1;            // Assigned state value

    // Terminator info
    std::string terminator;          // Original terminator instruction
    std::string condition;           // Condition for conditional branch
    int true_target = -1;            // Target if condition true
    int false_target = -1;           // Target if condition false

    // Switch statement info
    int switch_default = -1;                                    // Default case target
    std::string switch_condition;                               // Switch condition variable
    std::vector<std::pair<int, int>> switch_cases;              // (value, target_block_id) pairs
};

/**
 * Represents the entire control flow graph
 */
struct CFGInfo {
    std::string function_name;
    std::vector<BasicBlockInfo> blocks;
    int entry_block = 0;
    std::vector<int> exit_blocks;

    // Loop information
    std::vector<std::pair<int, int>> back_edges;  // (from, to) pairs
    std::unordered_set<int> loop_headers;

    // Exception handling information
    bool has_exception_handling = false;  // Function uses invoke/landingpad
    std::vector<int> landing_pads;        // IDs of landing pad blocks
    std::vector<int> invoke_blocks;       // IDs of blocks with invoke

    // Statistics
    int num_blocks = 0;
    int num_edges = 0;
    int num_loops = 0;
    int num_conditionals = 0;

    /**
     * Get block by ID
     */
    BasicBlockInfo* getBlock(int id) {
        if (id >= 0 && id < static_cast<int>(blocks.size())) {
            return &blocks[id];
        }
        return nullptr;
    }

    const BasicBlockInfo* getBlock(int id) const {
        if (id >= 0 && id < static_cast<int>(blocks.size())) {
            return &blocks[id];
        }
        return nullptr;
    }
};

/**
 * Configuration for CFF pass
 */
struct CFFConfig {
    bool enabled = true;
    double probability = 0.85;       // Probability of flattening a function

    int min_blocks = 3;              // Minimum blocks to flatten
    int max_blocks = 100;            // Maximum blocks to handle

    bool shuffle_states = true;      // Randomize state values
    bool add_bogus_cases = false;    // Add fake switch cases
    int bogus_case_count = 2;        // Number of fake cases

    bool preserve_loops = false;     // Try to preserve loop structure
    bool skip_small_functions = true;

    // State variable options
    std::string state_var_name = "_cff_state";
    bool use_obfuscated_state = false;  // XOR state values
};

/**
 * Result of flattening a function
 */
struct CFFResult {
    bool success = false;
    std::string error;

    int original_blocks = 0;
    int flattened_blocks = 0;
    int states_created = 0;

    std::vector<std::string> transformed_code;
};

/**
 * PHI node information for proper handling during CFF
 */
struct PhiNodeInfo {
    std::string result;                        // Result variable
    std::string type;                          // LLVM type (i32, i64, etc.)
    int block_id;                              // Block where PHI is defined
    std::vector<std::string> incoming_values;  // Values from predecessors
    std::vector<std::string> incoming_labels;  // Predecessor labels
};

/**
 * Base class for CFF analysis
 */
class CFGAnalyzer {
public:
    virtual ~CFGAnalyzer() = default;

    /**
     * Analyze a function and build CFG info
     */
    virtual std::optional<CFGInfo> analyze(const std::vector<std::string>& lines) = 0;

    /**
     * Identify loops in the CFG (back edges)
     */
    virtual void identifyLoops(CFGInfo& cfg) = 0;

    /**
     * Check if function is suitable for flattening
     */
    virtual bool isSuitable(const CFGInfo& cfg, const CFFConfig& config) {
        if (cfg.num_blocks < config.min_blocks) return false;
        if (cfg.num_blocks > config.max_blocks) return false;
        return true;
    }
};

/**
 * Base class for CFF transformation
 */
class CFFTransformation {
public:
    virtual ~CFFTransformation() = default;

    /**
     * Get transformation name
     */
    virtual std::string getName() const = 0;

    /**
     * Flatten a function's control flow
     */
    virtual CFFResult flatten(const CFGInfo& cfg, const CFFConfig& config) = 0;

protected:
    Logger logger_{"CFF"};

    /**
     * Assign state values to blocks
     */
    std::unordered_map<int, int> assignStates(const CFGInfo& cfg, const CFFConfig& config) {
        std::unordered_map<int, int> states;
        std::vector<int> block_order;

        // Collect all block IDs
        for (const auto& block : cfg.blocks) {
            block_order.push_back(block.id);
        }

        // Optionally shuffle
        if (config.shuffle_states) {
            // Fisher-Yates shuffle
            for (size_t i = block_order.size() - 1; i > 0; i--) {
                size_t j = GlobalRandom::nextInt(0, static_cast<int>(i));
                std::swap(block_order[i], block_order[j]);
            }
        }

        // Entry block always gets state 0
        states[cfg.entry_block] = 0;

        // Assign other states
        int next_state = 1;
        for (int block_id : block_order) {
            if (states.find(block_id) == states.end()) {
                states[block_id] = next_state++;
            }
        }

        // Exit state is special
        // END_STATE = number of blocks

        return states;
    }

    /**
     * Generate switch case for a block
     */
    virtual std::vector<std::string> generateCase(
        const BasicBlockInfo& block,
        const std::unordered_map<int, int>& states,
        int end_state,
        const CFFConfig& config) = 0;
};

/**
 * LLVM IR-specific CFF implementation
 */
class LLVMCFGAnalyzer : public CFGAnalyzer {
public:
    std::optional<CFGInfo> analyze(const std::vector<std::string>& lines) override;
    void identifyLoops(CFGInfo& cfg) override;

private:
    /**
     * Parse a function definition
     */
    bool parseFunction(const std::vector<std::string>& lines,
                       size_t start, size_t& end,
                       CFGInfo& cfg);

    /**
     * Parse a basic block
     */
    BasicBlockInfo parseBlock(const std::vector<std::string>& lines,
                              size_t start, size_t& end);

    /**
     * Parse terminator instruction
     */
    void parseTerminator(BasicBlockInfo& block, const std::string& line);

    /**
     * DFS for loop detection
     */
    void dfsLoops(CFGInfo& cfg, int node,
                  std::unordered_set<int>& visited,
                  std::unordered_set<int>& in_stack);
};

/**
 * LLVM IR CFF Transformation
 */
class LLVMCFFTransformation : public CFFTransformation {
public:
    std::string getName() const override { return "LLVM_CFF"; }

    CFFResult flatten(const CFGInfo& cfg, const CFFConfig& config) override;

protected:
    std::vector<std::string> generateCase(
        const BasicBlockInfo& block,
        const std::unordered_map<int, int>& states,
        int end_state,
        const CFFConfig& config) override;

private:
    /**
     * Generate the dispatcher structure
     */
    std::vector<std::string> generateDispatcher(
        const CFGInfo& cfg,
        const std::unordered_map<int, int>& states,
        const CFFConfig& config);

    /**
     * Generate state update for a terminator
     */
    std::vector<std::string> generateStateUpdate(
        const BasicBlockInfo& block,
        const std::unordered_map<int, int>& states,
        int end_state,
        const CFFConfig& config);

    /**
     * Generate PHI node handling (allocas at function entry)
     */
    std::vector<std::string> generatePhiHandling(
        const CFGInfo& cfg,
        const std::unordered_map<int, int>& states);

    /**
     * Generate PHI stores at end of predecessor blocks
     */
    std::vector<std::string> generatePhiStores(
        const BasicBlockInfo& block,
        const std::unordered_map<int, int>& states);

    /**
     * Generate PHI loads at merge points
     */
    std::vector<std::string> generatePhiLoads(const BasicBlockInfo& block);

    /**
     * Collect all PHI nodes from the CFG
     */
    void collectPhiNodes(const CFGInfo& cfg);

    int temp_counter_ = 0;
    std::string nextTemp() {
        return "%_cff_tmp" + std::to_string(temp_counter_++);
    }

    // PHI node tracking
    std::vector<PhiNodeInfo> phi_nodes_;
    std::unordered_map<std::string, std::string> phi_alloca_map_;   // PHI result -> alloca var
    std::unordered_map<std::string, std::string> phi_replacement_map_;  // PHI result -> loaded var

    // Entry block allocas (moved to entry_flat for dominance)
    std::vector<std::string> entry_allocas_;

    // Return value tracking
    std::string return_type_;           // Type of return value (void, i32, etc.)
    std::string return_alloca_;         // Alloca for storing return value
    bool has_return_value_ = false;     // Whether function returns a value
};

/**
 * CFF Pass for LLVM IR
 */
class LLVMCFFPass : public LLVMTransformationPass {
public:
    LLVMCFFPass() : analyzer_(), transformer_() {}

    std::string getName() const override { return "CFF"; }
    std::string getDescription() const override {
        return "Control Flow Flattening - converts CFG to state machine";
    }

    PassPriority getPriority() const override { return PassPriority::ControlFlow; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        cff_config_.enabled = config.enabled;
        cff_config_.probability = config.probability;
        return true;
    }

    void setCFFConfig(const CFFConfig& config) {
        cff_config_ = config;
    }

    TransformResult transformIR(std::vector<std::string>& lines) override;

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    LLVMCFGAnalyzer analyzer_;
    LLVMCFFTransformation transformer_;
    CFFConfig cff_config_;

    /**
     * Find function boundaries in IR
     */
    std::vector<std::pair<size_t, size_t>> findFunctions(
        const std::vector<std::string>& lines);

    /**
     * Extract function lines
     */
    std::vector<std::string> extractFunction(
        const std::vector<std::string>& lines,
        size_t start, size_t end);
};

} // namespace cff
} // namespace morphect

#endif // MORPHECT_CFF_BASE_HPP
