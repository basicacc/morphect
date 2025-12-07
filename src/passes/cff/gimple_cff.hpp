/**
 * Morphect - GIMPLE Control Flow Flattening
 *
 * Implements CFF for GCC GIMPLE IR.
 * Transforms function CFG into switch-based state machine.
 */

#ifndef MORPHECT_GIMPLE_CFF_HPP
#define MORPHECT_GIMPLE_CFF_HPP

// This file is only included when building the GCC plugin
#ifdef MORPHECT_GIMPLE_PLUGIN

#include "cff_base.hpp"
#include "opaque_predicates.hpp"

// GCC headers - order matters for GCC 15+
#include "gcc-plugin.h"
#include "tree.h"
#include "gimple.h"
#include "basic-block.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "tree-ssanames.h"
#include "tree-ssa-operands.h"
#include "tree-phinodes.h"
#include "gimple-ssa.h"
#include "ssa-iterators.h"
#include "cfgloop.h"
#include "dominance.h"
#include "tree-into-ssa.h"

// Helper for GCC 15+ compatibility - last_stmt was removed
static inline gimple* morphect_last_stmt(basic_block bb) {
    gimple_stmt_iterator gsi = gsi_last_bb(bb);
    if (gsi_end_p(gsi)) return nullptr;
    return gsi_stmt(gsi);
}

#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace morphect {
namespace cff {

/**
 * GIMPLE CFG Information
 * Extended from base CFGInfo with GIMPLE-specific data
 */
struct GimpleCFGInfo {
    function* fn = nullptr;
    std::string function_name;

    // Basic block mapping
    std::unordered_map<int, basic_block> index_to_bb;
    std::unordered_map<basic_block, int> bb_to_index;

    // Block information
    int num_blocks = 0;
    int num_conditionals = 0;
    int num_loops = 0;

    // Special blocks
    basic_block entry_bb = nullptr;
    std::vector<basic_block> exit_bbs;
    std::unordered_set<basic_block> loop_headers;
    std::unordered_set<basic_block> exception_blocks;

    // PHI nodes that need handling
    std::vector<std::pair<basic_block, gphi*>> phi_nodes;

    /**
     * Check if block is suitable for flattening
     */
    bool isSuitable() const {
        return num_blocks >= 3 && exception_blocks.empty();
    }
};

/**
 * GIMPLE CFG Analyzer
 * Analyzes GCC function to build CFG representation
 */
class GimpleCFGAnalyzer {
public:
    /**
     * Analyze a GIMPLE function's CFG
     */
    GimpleCFGInfo analyze(function* fn) {
        GimpleCFGInfo cfg;
        cfg.fn = fn;
        cfg.function_name = function_name(fn) ? function_name(fn) : "unknown";

        // Map basic blocks to indices
        int index = 0;
        basic_block bb;
        FOR_EACH_BB_FN(bb, fn) {
            cfg.index_to_bb[index] = bb;
            cfg.bb_to_index[bb] = index;
            index++;
        }
        cfg.num_blocks = index;

        // Find entry block
        cfg.entry_bb = ENTRY_BLOCK_PTR_FOR_FN(fn)->next_bb;

        // Analyze each block
        FOR_EACH_BB_FN(bb, fn) {
            analyzeBlock(bb, cfg);
        }

        // Identify loops using GCC's loop infrastructure if available
        identifyLoops(fn, cfg);

        return cfg;
    }

    /**
     * Check if function is suitable for CFF
     */
    bool isSuitable(const GimpleCFGInfo& cfg, const CFFConfig& config) {
        if (cfg.num_blocks < config.min_blocks) return false;
        if (cfg.num_blocks > config.max_blocks) return false;
        if (!cfg.exception_blocks.empty()) return false;  // Skip functions with exceptions
        return true;
    }

private:
    void analyzeBlock(basic_block bb, GimpleCFGInfo& cfg) {
        // Check for conditionals
        gimple* last = morphect_last_stmt(bb);
        if (last && gimple_code(last) == GIMPLE_COND) {
            cfg.num_conditionals++;
        }

        // Check for exception handling
        if (last) {
            if (gimple_code(last) == GIMPLE_RESX ||
                gimple_code(last) == GIMPLE_EH_DISPATCH) {
                cfg.exception_blocks.insert(bb);
            }
        }

        // Find PHI nodes
        for (gphi_iterator gpi = gsi_start_phis(bb); !gsi_end_p(gpi); gsi_next(&gpi)) {
            gphi* phi = gpi.phi();
            cfg.phi_nodes.push_back({bb, phi});
        }

        // Check for exit blocks (return statements)
        if (last && gimple_code(last) == GIMPLE_RETURN) {
            cfg.exit_bbs.push_back(bb);
        }
    }

    void identifyLoops(function* fn, GimpleCFGInfo& cfg) {
        // Use GCC's loop analysis if loops are available
        if (loops_for_fn(fn)) {
            for (auto loop : loops_list(fn, 0)) {
                if (loop && loop->header) {
                    cfg.loop_headers.insert(loop->header);
                    cfg.num_loops++;
                }
            }
        }
    }
};

/**
 * GIMPLE CFF Transformation
 * Transforms CFG into switch-based dispatcher
 */
class GimpleCFFTransformation {
public:
    /**
     * Flatten a function's control flow
     */
    bool flatten(function* fn, const GimpleCFGInfo& cfg, const CFFConfig& config) {
        if (!cfg.isSuitable()) return false;

        fn_ = fn;
        config_ = config;

        // Create state variable
        state_var_ = createStateVariable(fn);
        if (!state_var_) return false;

        // Assign states to blocks
        assignStates(cfg);

        // Handle PHI nodes first (convert to loads/stores)
        convertPhiNodes(cfg);

        // Create dispatcher block
        basic_block dispatcher = createDispatcher(cfg);
        if (!dispatcher) return false;

        // Transform each original block
        for (const auto& [idx, bb] : cfg.index_to_bb) {
            transformBlock(bb, cfg);
        }

        // Redirect entry to dispatcher
        redirectEntry(cfg, dispatcher);

        // Update SSA form
        update_ssa(TODO_update_ssa);

        return true;
    }

private:
    function* fn_ = nullptr;
    CFFConfig config_;
    tree state_var_ = NULL_TREE;
    std::unordered_map<basic_block, int> bb_to_state_;
    int end_state_ = 0;
    int temp_counter_ = 0;

    /**
     * Create the state variable (integer)
     */
    tree createStateVariable(function* fn) {
        // Create an integer type state variable
        tree int_type = integer_type_node;
        tree state = create_tmp_var(int_type, "cff_state");

        // Mark as addressable for stores
        TREE_ADDRESSABLE(state) = 1;

        return state;
    }

    /**
     * Assign state numbers to basic blocks
     */
    void assignStates(const GimpleCFGInfo& cfg) {
        int state = 0;

        // Entry block gets state 0
        if (cfg.entry_bb) {
            bb_to_state_[cfg.entry_bb] = state++;
        }

        // Assign remaining blocks
        for (const auto& [idx, bb] : cfg.index_to_bb) {
            if (bb != cfg.entry_bb && bb_to_state_.find(bb) == bb_to_state_.end()) {
                bb_to_state_[bb] = state++;
            }
        }

        // End state
        end_state_ = state;
    }

    /**
     * Convert PHI nodes to explicit loads/stores
     */
    void convertPhiNodes(const GimpleCFGInfo& cfg) {
        for (const auto& [bb, phi] : cfg.phi_nodes) {
            tree result = gimple_phi_result(phi);
            tree type = TREE_TYPE(result);

            // Create temporary variable to hold PHI value
            tree phi_var = create_tmp_var(type, "phi_tmp");
            TREE_ADDRESSABLE(phi_var) = 1;

            // For each predecessor, add store at end of predecessor
            for (unsigned i = 0; i < gimple_phi_num_args(phi); i++) {
                edge e = gimple_phi_arg_edge(phi, i);
                tree arg = gimple_phi_arg_def(phi, i);

                // Create store in predecessor
                gimple* store = gimple_build_assign(phi_var, arg);
                gimple_stmt_iterator gsi = gsi_last_bb(e->src);

                // Insert before terminator
                if (!gsi_end_p(gsi)) {
                    gimple* last = gsi_stmt(gsi);
                    if (gimple_code(last) == GIMPLE_COND ||
                        gimple_code(last) == GIMPLE_SWITCH ||
                        gimple_code(last) == GIMPLE_GOTO ||
                        gimple_code(last) == GIMPLE_RETURN) {
                        gsi_insert_before(&gsi, store, GSI_SAME_STMT);
                    } else {
                        gsi_insert_after(&gsi, store, GSI_NEW_STMT);
                    }
                }
            }

            // Replace PHI with load
            tree loaded = make_ssa_name(type);
            gimple* load = gimple_build_assign(loaded, phi_var);

            gimple_stmt_iterator phi_gsi = gsi_for_stmt(phi);
            gsi_insert_before(&phi_gsi, load, GSI_SAME_STMT);

            // Replace uses of PHI result with loaded value
            imm_use_iterator iter;
            use_operand_p use_p;
            FOR_EACH_IMM_USE_FAST(use_p, iter, result) {
                SET_USE(use_p, loaded);
            }

            // Remove PHI
            remove_phi_node(&phi_gsi, true);
        }
    }

    /**
     * Create the central dispatcher block
     */
    basic_block createDispatcher(const GimpleCFGInfo& cfg) {
        // Create new basic block for dispatcher
        basic_block dispatcher = create_empty_bb(cfg.entry_bb);

        // Load state variable
        tree state_val = make_ssa_name(integer_type_node);
        gimple* load = gimple_build_assign(state_val, state_var_);
        gimple_stmt_iterator gsi = gsi_last_bb(dispatcher);
        gsi_insert_after(&gsi, load, GSI_NEW_STMT);

        // Create switch statement
        int num_states = static_cast<int>(bb_to_state_.size());
        gswitch* sw = gimple_build_switch_nlabels(num_states, state_val, NULL_TREE);

        // Add case for each state
        int case_idx = 0;
        for (const auto& [bb, state] : bb_to_state_) {
            tree case_label = create_artificial_label(UNKNOWN_LOCATION);
            DECL_NONLOCAL(case_label) = 0;

            tree case_low = build_int_cst(integer_type_node, state);
            tree case_tree = build_case_label(case_low, NULL_TREE, case_label);

            gimple_switch_set_label(sw, case_idx++, case_tree);

            // Create edge from dispatcher to block
            make_edge(dispatcher, bb, 0);
        }

        // Insert switch
        gsi = gsi_last_bb(dispatcher);
        gsi_insert_after(&gsi, sw, GSI_NEW_STMT);

        return dispatcher;
    }

    /**
     * Transform a basic block - replace terminator with state update + goto dispatcher
     */
    void transformBlock(basic_block bb, const GimpleCFGInfo& cfg) {
        gimple* last = morphect_last_stmt(bb);
        if (!last) return;

        gimple_stmt_iterator gsi = gsi_last_bb(bb);
        location_t loc = gimple_location(last);

        // Determine next state based on terminator type
        switch (gimple_code(last)) {
            case GIMPLE_COND: {
                // Conditional: state = cond ? true_state : false_state
                gcond* cond = as_a<gcond*>(last);
                edge true_edge = EDGE_SUCC(bb, 0);
                edge false_edge = EDGE_SUCC(bb, 1);

                if (true_edge->flags & EDGE_FALSE_VALUE) {
                    std::swap(true_edge, false_edge);
                }

                int true_state = bb_to_state_[true_edge->dest];
                int false_state = bb_to_state_[false_edge->dest];

                // Build condition expression
                tree cond_expr = build2(gimple_cond_code(cond),
                                       boolean_type_node,
                                       gimple_cond_lhs(cond),
                                       gimple_cond_rhs(cond));

                // state = cond ? true_state : false_state
                tree true_val = build_int_cst(integer_type_node, true_state);
                tree false_val = build_int_cst(integer_type_node, false_state);
                tree select = build3(COND_EXPR, integer_type_node,
                                    cond_expr, true_val, false_val);

                tree new_state = make_ssa_name(integer_type_node);
                gimple* assign = gimple_build_assign(new_state, select);
                gimple_set_location(assign, loc);
                gsi_insert_before(&gsi, assign, GSI_SAME_STMT);

                // Store new state
                gimple* store = gimple_build_assign(state_var_, new_state);
                gimple_set_location(store, loc);
                gsi_insert_before(&gsi, store, GSI_SAME_STMT);

                // Remove original conditional
                gsi_remove(&gsi, true);
                break;
            }

            case GIMPLE_GOTO: {
                // Unconditional jump - set state to target
                tree dest = gimple_goto_dest(last);
                // Find target block and its state
                edge e;
                edge_iterator ei;
                FOR_EACH_EDGE(e, ei, bb->succs) {
                    if (bb_to_state_.find(e->dest) != bb_to_state_.end()) {
                        int next_state = bb_to_state_[e->dest];
                        gimple* store = gimple_build_assign(state_var_,
                            build_int_cst(integer_type_node, next_state));
                        gimple_set_location(store, loc);
                        gsi_insert_before(&gsi, store, GSI_SAME_STMT);
                        break;
                    }
                }
                gsi_remove(&gsi, true);
                break;
            }

            case GIMPLE_RETURN: {
                // Return - set state to end_state
                gimple* store = gimple_build_assign(state_var_,
                    build_int_cst(integer_type_node, end_state_));
                gimple_set_location(store, loc);
                gsi_insert_before(&gsi, store, GSI_SAME_STMT);

                // Keep return for now, will be moved to end block later
                break;
            }

            default:
                // For blocks without explicit terminator (fallthrough)
                if (single_succ_p(bb)) {
                    basic_block succ = single_succ(bb);
                    if (bb_to_state_.find(succ) != bb_to_state_.end()) {
                        int next_state = bb_to_state_[succ];
                        gimple* store = gimple_build_assign(state_var_,
                            build_int_cst(integer_type_node, next_state));
                        gimple_set_location(store, loc);
                        gsi_insert_after(&gsi, store, GSI_NEW_STMT);
                    }
                }
                break;
        }
    }

    /**
     * Redirect entry to go through dispatcher
     */
    void redirectEntry(const GimpleCFGInfo& cfg, basic_block dispatcher) {
        // Initialize state to entry block's state
        basic_block entry = ENTRY_BLOCK_PTR_FOR_FN(fn_)->next_bb;

        int entry_state = bb_to_state_[cfg.entry_bb];

        // Add initialization at entry
        gimple_stmt_iterator gsi = gsi_start_bb(entry);
        gimple* init = gimple_build_assign(state_var_,
            build_int_cst(integer_type_node, entry_state));
        gsi_insert_before(&gsi, init, GSI_SAME_STMT);

        // Add jump to dispatcher
        gimple* jump = gimple_build_goto(
            gimple_block_label(dispatcher));
        gsi_insert_after(&gsi, jump, GSI_NEW_STMT);
    }
};

/**
 * GIMPLE Bogus Control Flow
 * Inserts fake branches with opaque predicates
 */
class GimpleBogusControlFlow {
public:
    /**
     * Insert bogus control flow into function
     */
    bool insertBogusFlow(function* fn, const BogusConfig& config) {
        if (!config.enabled) return false;

        fn_ = fn;
        config_ = config;
        insertions_ = 0;

        basic_block bb;
        std::vector<basic_block> candidates;

        // Collect candidate blocks
        FOR_EACH_BB_FN(bb, fn) {
            if (isCandidateBlock(bb)) {
                candidates.push_back(bb);
            }
        }

        // Insert bogus branches
        int num_insertions = GlobalRandom::nextInt(config.min_insertions,
                                                   config.max_insertions);

        for (int i = 0; i < num_insertions && !candidates.empty(); i++) {
            // Select random block
            size_t idx = GlobalRandom::nextSize(candidates.size());
            basic_block target = candidates[idx];

            if (GlobalRandom::decide(config.probability)) {
                if (insertBogusBranch(target)) {
                    insertions_++;
                }
            }

            // Remove from candidates to avoid double-processing
            candidates.erase(candidates.begin() + idx);
        }

        if (insertions_ > 0) {
            update_ssa(TODO_update_ssa);
        }

        return insertions_ > 0;
    }

    int getInsertions() const { return insertions_; }

private:
    function* fn_ = nullptr;
    BogusConfig config_;
    int insertions_ = 0;
    int temp_counter_ = 0;

    bool isCandidateBlock(basic_block bb) {
        // Skip entry and exit blocks
        if (bb == ENTRY_BLOCK_PTR_FOR_FN(fn_)) return false;
        if (bb == EXIT_BLOCK_PTR_FOR_FN(fn_)) return false;

        // Skip blocks with exception handling
        gimple* last = morphect_last_stmt(bb);
        if (last) {
            enum gimple_code code = gimple_code(last);
            if (code == GIMPLE_RESX || code == GIMPLE_EH_DISPATCH) {
                return false;
            }
        }

        // Need at least one statement
        gimple_stmt_iterator gsi = gsi_start_bb(bb);
        return !gsi_end_p(gsi);
    }

    bool insertBogusBranch(basic_block bb) {
        // Get first non-PHI statement
        gimple_stmt_iterator gsi = gsi_start_nondebug_after_labels_bb(bb);
        if (gsi_end_p(gsi)) return false;

        location_t loc = gimple_location(gsi_stmt(gsi));

        // Generate opaque predicate (always true)
        tree predicate = generateOpaquePredicate(bb, loc);
        if (!predicate) return false;

        // Create fake block
        basic_block fake_bb = createFakeBlock(bb, loc);
        if (!fake_bb) return false;

        // Create merge block (rest of original code)
        basic_block merge_bb = split_block(bb, gsi_stmt(gsi))->dest;

        // Insert conditional branch
        gcond* cond = gimple_build_cond(NE_EXPR, predicate,
                                        build_int_cst(TREE_TYPE(predicate), 0),
                                        NULL_TREE, NULL_TREE);
        gimple_set_location(cond, loc);

        gsi = gsi_last_bb(bb);
        gsi_insert_after(&gsi, cond, GSI_NEW_STMT);

        // Create edges
        edge true_edge = make_edge(bb, merge_bb, EDGE_TRUE_VALUE);
        edge false_edge = make_edge(bb, fake_bb, EDGE_FALSE_VALUE);
        make_edge(fake_bb, merge_bb, EDGE_FALLTHRU);

        return true;
    }

    tree generateOpaquePredicate(basic_block bb, location_t loc) {
        // Generate: (x * (x + 1)) % 2 == 0  (always true)
        // Use a variable from the function if available, otherwise use constant

        tree x = NULL_TREE;

        // Try to find an integer variable in scope
        gimple_stmt_iterator gsi;
        for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
            gimple* stmt = gsi_stmt(gsi);
            if (is_gimple_assign(stmt)) {
                tree lhs = gimple_assign_lhs(stmt);
                if (INTEGRAL_TYPE_P(TREE_TYPE(lhs))) {
                    x = lhs;
                    break;
                }
            }
        }

        // Fallback to constant
        if (!x) {
            x = build_int_cst(integer_type_node, GlobalRandom::nextInt(1, 1000));
        }

        tree int_type = integer_type_node;

        // x + 1
        tree x_plus_1 = make_ssa_name(int_type);
        gimple* g1 = gimple_build_assign(x_plus_1, PLUS_EXPR, x,
                                         build_int_cst(int_type, 1));

        // x * (x + 1)
        tree product = make_ssa_name(int_type);
        gimple* g2 = gimple_build_assign(product, MULT_EXPR, x, x_plus_1);

        // product % 2
        tree mod_result = make_ssa_name(int_type);
        gimple* g3 = gimple_build_assign(mod_result, TRUNC_MOD_EXPR, product,
                                         build_int_cst(int_type, 2));

        // mod_result == 0
        tree cmp_result = make_ssa_name(boolean_type_node);
        gimple* g4 = gimple_build_assign(cmp_result, EQ_EXPR, mod_result,
                                         build_int_cst(int_type, 0));

        // Insert all statements
        gimple_stmt_iterator gsi_insert = gsi_last_bb(bb);
        gsi_insert_after(&gsi_insert, g1, GSI_NEW_STMT);
        gsi_insert_after(&gsi_insert, g2, GSI_NEW_STMT);
        gsi_insert_after(&gsi_insert, g3, GSI_NEW_STMT);
        gsi_insert_after(&gsi_insert, g4, GSI_NEW_STMT);

        return cmp_result;
    }

    basic_block createFakeBlock(basic_block after, location_t loc) {
        basic_block fake = create_empty_bb(after);

        if (config_.generate_dead_code) {
            // Generate some dead code
            tree int_type = integer_type_node;

            for (int i = 0; i < 3; i++) {
                tree tmp = make_ssa_name(int_type);
                int val1 = GlobalRandom::nextInt(1, 100);
                int val2 = GlobalRandom::nextInt(1, 100);

                gimple* dead = gimple_build_assign(tmp, PLUS_EXPR,
                    build_int_cst(int_type, val1),
                    build_int_cst(int_type, val2));
                gimple_set_location(dead, loc);

                gimple_stmt_iterator gsi = gsi_last_bb(fake);
                gsi_insert_after(&gsi, dead, GSI_NEW_STMT);
            }
        }

        return fake;
    }
};

/**
 * GIMPLE CFF Pass
 * Integrates with pass manager
 */
class GimpleCFFPass {
public:
    GimpleCFFPass() = default;

    void setCFFConfig(const CFFConfig& config) {
        cff_config_ = config;
    }

    void setBogusConfig(const BogusConfig& config) {
        bogus_config_ = config;
    }

    bool runOnFunction(function* fn) {
        bool modified = false;

        // Analyze CFG
        GimpleCFGAnalyzer analyzer;
        auto cfg = analyzer.analyze(fn);

        // Apply CFF if enabled and suitable
        if (cff_config_.enabled && analyzer.isSuitable(cfg, cff_config_)) {
            if (GlobalRandom::decide(cff_config_.probability)) {
                GimpleCFFTransformation transformer;
                if (transformer.flatten(fn, cfg, cff_config_)) {
                    modified = true;
                    functions_flattened_++;
                }
            }
        }

        // Apply Bogus CF if enabled
        if (bogus_config_.enabled) {
            GimpleBogusControlFlow bogus;
            if (bogus.insertBogusFlow(fn, bogus_config_)) {
                modified = true;
                bogus_insertions_ += bogus.getInsertions();
            }
        }

        return modified;
    }

    int getFunctionsFlattened() const { return functions_flattened_; }
    int getBogusInsertions() const { return bogus_insertions_; }

private:
    CFFConfig cff_config_;
    BogusConfig bogus_config_;
    int functions_flattened_ = 0;
    int bogus_insertions_ = 0;
};

} // namespace cff
} // namespace morphect

#endif // MORPHECT_GIMPLE_PLUGIN

#endif // MORPHECT_GIMPLE_CFF_HPP
