/**
 * Morphect - Indirect Branch Obfuscation Base
 *
 * Base definitions for indirect branch transformations.
 * Converts direct jumps/branches to indirect jumps via lookup tables.
 */

#ifndef MORPHECT_INDIRECT_BRANCH_BASE_HPP
#define MORPHECT_INDIRECT_BRANCH_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"
#include "../mba/mba_base.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace morphect {
namespace control_flow {

/**
 * Strategy for computing jump table index
 */
enum class IndexObfStrategy {
    Direct,         // Direct index (no obfuscation)
    XOR,            // index ^ key
    MBA,            // MBA expression for index
    LinearTransform // (a * index + b) mod table_size
};

/**
 * Represents a single jump table entry
 */
struct JumpTableEntry {
    int original_index = 0;      // Original logical index
    int obfuscated_index = 0;    // Index after obfuscation
    std::string target_label;    // Target block/label
    uint64_t target_address = 0; // For assembly (address or offset)

    // For decoy entries
    bool is_decoy = false;       // True if this is a fake entry
};

/**
 * Represents a complete jump table
 */
struct JumpTable {
    std::string table_name;              // Name/label of the table
    std::vector<JumpTableEntry> entries;

    // Obfuscation parameters
    IndexObfStrategy index_strategy = IndexObfStrategy::XOR;
    uint64_t xor_key = 0;                // For XOR strategy
    int64_t linear_a = 1;                // For linear: (a * idx + b) % size
    int64_t linear_b = 0;

    // MBA parameters (if using MBA strategy)
    std::vector<int64_t> mba_params;

    // Table properties
    size_t table_size = 0;               // Actual size (may include decoys)
    size_t real_entries = 0;             // Number of real entries
    bool has_decoys = false;

    /**
     * Get entry by obfuscated index
     */
    const JumpTableEntry* getEntry(int obf_index) const {
        for (const auto& entry : entries) {
            if (entry.obfuscated_index == obf_index) {
                return &entry;
            }
        }
        return nullptr;
    }

    /**
     * Get entry by original index
     */
    const JumpTableEntry* getEntryByOriginal(int orig_index) const {
        for (const auto& entry : entries) {
            if (entry.original_index == orig_index) {
                return &entry;
            }
        }
        return nullptr;
    }
};

/**
 * Configuration for indirect branch pass
 */
struct IndirectBranchConfig {
    bool enabled = true;
    double probability = 0.8;            // Probability of transforming a branch

    // Index obfuscation
    IndexObfStrategy index_strategy = IndexObfStrategy::XOR;
    bool use_mba_for_index = true;       // Use MBA for index calculation
    int mba_complexity = 2;              // MBA depth/complexity

    // Table options
    bool add_decoy_entries = true;       // Add fake table entries
    int min_decoy_count = 1;
    int max_decoy_count = 3;
    bool shuffle_entries = true;         // Randomize entry order

    // Target-specific
    bool use_pic = true;                 // Position-independent code (relative offsets)

    // Naming
    std::string table_prefix = "_jt_";
    std::string index_var_prefix = "_idx_";
};

/**
 * Result of indirect branch transformation
 */
struct IndirectBranchResult {
    bool success = false;
    std::string error;

    int branches_transformed = 0;
    int tables_created = 0;
    int decoy_entries_added = 0;

    std::vector<JumpTable> tables;
    std::vector<std::string> transformed_code;
};

/**
 * Information about a branch to transform
 */
struct BranchInfo {
    int id;                              // Unique ID
    std::string source_label;            // Block containing the branch
    std::vector<std::string> targets;    // Target labels

    // For conditional branches
    bool is_conditional = false;
    std::string condition;               // Condition variable/expression

    // For switch statements
    bool is_switch = false;
    std::vector<int64_t> case_values;    // Case values for switch
    std::string default_target;          // Default case target

    // Original instruction
    std::string original_instruction;
    int line_number = -1;
};

/**
 * Base class for indirect branch analysis
 */
class IndirectBranchAnalyzer {
public:
    virtual ~IndirectBranchAnalyzer() = default;

    /**
     * Find all branches suitable for transformation
     */
    virtual std::vector<BranchInfo> findBranches(
        const std::vector<std::string>& lines) = 0;

    /**
     * Check if a branch is suitable for indirect transformation
     */
    virtual bool isSuitable(const BranchInfo& branch,
                           const IndirectBranchConfig& config) {
        // Switches are always suitable
        if (branch.is_switch && branch.targets.size() >= 2) {
            return true;
        }
        // Conditional branches with 2 targets
        if (branch.is_conditional && branch.targets.size() == 2) {
            return true;
        }
        // Unconditional branch to single target - transform for obfuscation
        if (!branch.is_conditional && branch.targets.size() == 1) {
            return GlobalRandom::nextDouble() < config.probability;
        }
        return false;
    }
};

/**
 * Base class for indirect branch transformation
 */
class IndirectBranchTransformation {
public:
    virtual ~IndirectBranchTransformation() = default;

    /**
     * Get transformation name
     */
    virtual std::string getName() const = 0;

    /**
     * Transform branches to use indirect jumps
     */
    virtual IndirectBranchResult transform(
        const std::vector<std::string>& lines,
        const std::vector<BranchInfo>& branches,
        const IndirectBranchConfig& config) = 0;

protected:
    Logger logger_{"IndirectBranch"};
    int table_counter_ = 0;
    int temp_counter_ = 0;

    std::string nextTableName(const std::string& prefix) {
        return prefix + std::to_string(table_counter_++);
    }

    std::string nextTemp() {
        return "_ib_tmp" + std::to_string(temp_counter_++);
    }

    /**
     * Create a jump table for a branch
     */
    virtual JumpTable createJumpTable(
        const BranchInfo& branch,
        const IndirectBranchConfig& config) {

        JumpTable table;
        table.table_name = nextTableName(config.table_prefix);
        table.index_strategy = config.index_strategy;
        table.real_entries = branch.targets.size();

        // Create entries for each target
        for (size_t i = 0; i < branch.targets.size(); i++) {
            JumpTableEntry entry;
            entry.original_index = static_cast<int>(i);
            entry.target_label = branch.targets[i];
            entry.is_decoy = false;
            table.entries.push_back(entry);
        }

        // Add decoy entries if configured
        if (config.add_decoy_entries) {
            int decoy_count = GlobalRandom::nextInt(
                config.min_decoy_count,
                config.max_decoy_count + 1);

            for (int i = 0; i < decoy_count; i++) {
                JumpTableEntry decoy;
                decoy.original_index = -1;  // Invalid
                // Point to a random real target (for safety)
                int target_idx = GlobalRandom::nextInt(0, static_cast<int>(branch.targets.size()) - 1);
                decoy.target_label = branch.targets[target_idx];
                decoy.is_decoy = true;
                table.entries.push_back(decoy);
            }
            table.has_decoys = true;
        }

        table.table_size = table.entries.size();

        // Apply obfuscation to indices
        obfuscateIndices(table, config);

        // Shuffle if configured
        if (config.shuffle_entries) {
            shuffleEntries(table);
        }

        return table;
    }

    /**
     * Obfuscate table indices based on strategy
     */
    virtual void obfuscateIndices(JumpTable& table,
                                  const IndirectBranchConfig& config) {
        switch (config.index_strategy) {
            case IndexObfStrategy::Direct:
                // No obfuscation - indices stay the same
                for (auto& entry : table.entries) {
                    if (!entry.is_decoy) {
                        entry.obfuscated_index = entry.original_index;
                    } else {
                        // Decoys get indices after real entries
                        entry.obfuscated_index = static_cast<int>(table.real_entries) +
                            GlobalRandom::nextInt(0, 1000);
                    }
                }
                break;

            case IndexObfStrategy::XOR:
                table.xor_key = GlobalRandom::nextInt(1, 0xFFFF);
                for (auto& entry : table.entries) {
                    if (!entry.is_decoy) {
                        entry.obfuscated_index = entry.original_index ^
                            static_cast<int>(table.xor_key);
                    } else {
                        entry.obfuscated_index = GlobalRandom::nextInt(0, 0xFFFF);
                    }
                }
                break;

            case IndexObfStrategy::LinearTransform:
                // Use (a * idx + b) % table_size
                table.linear_a = GlobalRandom::nextInt(1, 100);
                // Ensure a is coprime with table_size for invertibility
                while (gcd(table.linear_a, static_cast<int64_t>(table.table_size)) != 1) {
                    table.linear_a = GlobalRandom::nextInt(1, 100);
                }
                table.linear_b = GlobalRandom::nextInt(0, static_cast<int>(table.table_size));

                for (auto& entry : table.entries) {
                    if (!entry.is_decoy) {
                        entry.obfuscated_index = static_cast<int>(
                            (table.linear_a * entry.original_index + table.linear_b) %
                            static_cast<int64_t>(table.table_size));
                    } else {
                        entry.obfuscated_index = GlobalRandom::nextInt(0,
                            static_cast<int>(table.table_size) * 2);
                    }
                }
                break;

            case IndexObfStrategy::MBA:
                // MBA obfuscation will be handled by generateIndexCalculation
                table.xor_key = GlobalRandom::nextInt(1, 0xFFFF);
                for (auto& entry : table.entries) {
                    if (!entry.is_decoy) {
                        entry.obfuscated_index = entry.original_index ^
                            static_cast<int>(table.xor_key);
                    } else {
                        entry.obfuscated_index = GlobalRandom::nextInt(0, 0xFFFF);
                    }
                }
                break;
        }
    }

    /**
     * Shuffle table entries
     */
    void shuffleEntries(JumpTable& table) {
        // Fisher-Yates shuffle
        for (size_t i = table.entries.size() - 1; i > 0; i--) {
            size_t j = GlobalRandom::nextInt(0, static_cast<int>(i));
            std::swap(table.entries[i], table.entries[j]);
        }
    }

    /**
     * Generate code to calculate obfuscated index
     */
    virtual std::vector<std::string> generateIndexCalculation(
        const JumpTable& table,
        const std::string& original_index_var,
        const std::string& result_var,
        const IndirectBranchConfig& config) = 0;

    /**
     * Greatest common divisor for linear transform
     */
    static int64_t gcd(int64_t a, int64_t b) {
        while (b != 0) {
            int64_t t = b;
            b = a % b;
            a = t;
        }
        return a;
    }
};

} // namespace control_flow
} // namespace morphect

#endif // MORPHECT_INDIRECT_BRANCH_BASE_HPP
