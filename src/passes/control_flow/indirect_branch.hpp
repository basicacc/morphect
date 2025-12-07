/**
 * Morphect - Indirect Branch Obfuscation for LLVM IR
 *
 * Transforms direct branches into indirect jumps via lookup tables.
 *
 * Original:
 *   br label %target
 *   br i1 %cond, label %true_bb, label %false_bb
 *
 * Transformed:
 *   ; Compute obfuscated index
 *   %idx = ... MBA expression to compute index ...
 *   ; Load target from jump table
 *   %target_ptr = getelementptr [N x i8*], [N x i8*]* @jump_table, i64 0, i64 %idx
 *   %target = load i8*, i8** %target_ptr
 *   indirectbr i8* %target, [label %bb1, label %bb2, ...]
 */

#ifndef MORPHECT_INDIRECT_BRANCH_HPP
#define MORPHECT_INDIRECT_BRANCH_HPP

#include "indirect_branch_base.hpp"
#include "../mba/mba.hpp"

#include <regex>
#include <sstream>
#include <algorithm>

namespace morphect {
namespace control_flow {

/**
 * LLVM IR Branch Analyzer
 */
class LLVMBranchAnalyzer : public IndirectBranchAnalyzer {
public:
    std::vector<BranchInfo> findBranches(
        const std::vector<std::string>& lines) override {

        std::vector<BranchInfo> branches;
        int branch_id = 0;
        std::string current_label;

        // Patterns for branch instructions
        std::regex unconditional_br(R"(^\s*br\s+label\s+%(\w+)\s*$)");
        std::regex conditional_br(
            R"(^\s*br\s+i1\s+(%\w+),\s*label\s+%(\w+),\s*label\s+%(\w+)\s*$)");
        std::regex switch_br(R"(^\s*switch\s+)");
        std::regex label_def(R"(^(\w+):)");

        for (size_t i = 0; i < lines.size(); i++) {
            const std::string& line = lines[i];
            std::smatch match;

            // Track current label
            if (std::regex_search(line, match, label_def)) {
                current_label = match[1].str();
                continue;
            }

            // Unconditional branch
            if (std::regex_search(line, match, unconditional_br)) {
                BranchInfo info;
                info.id = branch_id++;
                info.source_label = current_label;
                info.targets.push_back(match[1].str());
                info.is_conditional = false;
                info.is_switch = false;
                info.original_instruction = line;
                info.line_number = static_cast<int>(i);
                branches.push_back(info);
                continue;
            }

            // Conditional branch
            if (std::regex_search(line, match, conditional_br)) {
                BranchInfo info;
                info.id = branch_id++;
                info.source_label = current_label;
                info.condition = match[1].str();
                info.targets.push_back(match[2].str());  // true target
                info.targets.push_back(match[3].str());  // false target
                info.is_conditional = true;
                info.is_switch = false;
                info.original_instruction = line;
                info.line_number = static_cast<int>(i);
                branches.push_back(info);
                continue;
            }

            // Switch statement (multi-line)
            if (std::regex_search(line, switch_br)) {
                BranchInfo info = parseSwitch(lines, i);
                info.id = branch_id++;
                info.source_label = current_label;
                branches.push_back(info);
                // Skip parsed lines
                while (i < lines.size() && lines[i].find(']') == std::string::npos) {
                    i++;
                }
                continue;
            }
        }

        return branches;
    }

private:
    BranchInfo parseSwitch(const std::vector<std::string>& lines, size_t start) {
        BranchInfo info;
        info.is_switch = true;
        info.is_conditional = false;
        info.line_number = static_cast<int>(start);

        // Parse switch statement
        // switch i32 %val, label %default [
        //   i32 0, label %case0
        //   i32 1, label %case1
        // ]
        std::regex switch_head(R"(switch\s+i\d+\s+(%\w+),\s*label\s+%(\w+)\s*\[)");
        std::regex case_entry(R"(i\d+\s+(-?\d+),\s*label\s+%(\w+))");

        std::string full_switch;
        for (size_t i = start; i < lines.size(); i++) {
            full_switch += lines[i] + " ";
            if (lines[i].find(']') != std::string::npos) break;
        }

        info.original_instruction = full_switch;

        std::smatch match;
        if (std::regex_search(full_switch, match, switch_head)) {
            info.condition = match[1].str();
            info.default_target = match[2].str();
            info.targets.push_back(match[2].str());  // Default first
        }

        // Find all case entries
        std::string::const_iterator search_start = full_switch.cbegin();
        while (std::regex_search(search_start, full_switch.cend(), match, case_entry)) {
            info.case_values.push_back(std::stoll(match[1].str()));
            info.targets.push_back(match[2].str());
            search_start = match.suffix().first;
        }

        return info;
    }
};

/**
 * LLVM IR Indirect Branch Transformation
 */
class LLVMIndirectBranchTransformation : public IndirectBranchTransformation {
public:
    std::string getName() const override { return "LLVM_IndirectBranch"; }

    IndirectBranchResult transform(
        const std::vector<std::string>& lines,
        const std::vector<BranchInfo>& branches,
        const IndirectBranchConfig& config) override {

        IndirectBranchResult result;
        result.transformed_code = lines;

        // Track which lines to replace
        std::vector<std::pair<int, std::vector<std::string>>> replacements;

        // Process each branch
        for (const auto& branch : branches) {
            if (!isBranchSuitable(branch, config)) {
                continue;
            }

            if (GlobalRandom::nextDouble() > config.probability) {
                continue;
            }

            // Create jump table for this branch
            JumpTable table = createJumpTable(branch, config);
            result.tables.push_back(table);
            result.tables_created++;

            // Generate replacement code
            std::vector<std::string> replacement;

            if (branch.is_switch) {
                replacement = transformSwitch(branch, table, config);
            } else if (branch.is_conditional) {
                replacement = transformConditionalBranch(branch, table, config);
            } else {
                replacement = transformUnconditionalBranch(branch, table, config);
            }

            replacements.push_back({branch.line_number, replacement});
            result.branches_transformed++;
            result.decoy_entries_added += static_cast<int>(table.table_size - table.real_entries);
        }

        // Apply replacements in reverse order to preserve line numbers
        std::sort(replacements.begin(), replacements.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        for (const auto& [line_num, replacement] : replacements) {
            if (line_num >= 0 && line_num < static_cast<int>(result.transformed_code.size())) {
                result.transformed_code.erase(
                    result.transformed_code.begin() + line_num);
                result.transformed_code.insert(
                    result.transformed_code.begin() + line_num,
                    replacement.begin(), replacement.end());
            }
        }

        // Insert jump table declarations at module level
        insertJumpTableDeclarations(result);

        result.success = true;
        return result;
    }

protected:
    /**
     * Check if a branch is suitable for transformation (non-virtual helper)
     */
    bool isBranchSuitable(const BranchInfo& branch,
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

    std::vector<std::string> generateIndexCalculation(
        const JumpTable& table,
        const std::string& original_index_var,
        const std::string& result_var,
        const IndirectBranchConfig& config) override {

        std::vector<std::string> code;
        std::string idx_var = "%" + nextTemp();

        switch (table.index_strategy) {
            case IndexObfStrategy::Direct:
                // No transformation needed
                code.push_back("  " + result_var + " = add i64 " +
                    original_index_var + ", 0");
                break;

            case IndexObfStrategy::XOR:
                if (config.use_mba_for_index) {
                    // Use MBA for XOR operation
                    code = generateMBAIndexXOR(original_index_var, result_var,
                        static_cast<int64_t>(table.xor_key));
                } else {
                    // Simple XOR
                    code.push_back("  " + result_var + " = xor i64 " +
                        original_index_var + ", " + std::to_string(table.xor_key));
                }
                break;

            case IndexObfStrategy::LinearTransform: {
                // result = (a * idx + b) % size
                std::string mul_var = "%" + nextTemp();
                std::string add_var = "%" + nextTemp();
                code.push_back("  " + mul_var + " = mul i64 " +
                    original_index_var + ", " + std::to_string(table.linear_a));
                code.push_back("  " + add_var + " = add i64 " +
                    mul_var + ", " + std::to_string(table.linear_b));
                code.push_back("  " + result_var + " = urem i64 " +
                    add_var + ", " + std::to_string(table.table_size));
                break;
            }

            case IndexObfStrategy::MBA:
                // Use MBA for the XOR with additional complexity
                code = generateMBAIndexXOR(original_index_var, result_var,
                    static_cast<int64_t>(table.xor_key));
                break;
        }

        return code;
    }

private:
    /**
     * Generate MBA-based XOR for index calculation
     */
    std::vector<std::string> generateMBAIndexXOR(
        const std::string& idx_var,
        const std::string& result_var,
        int64_t xor_key) {

        std::vector<std::string> code;

        // Use the identity: a ^ b = (a | b) - (a & b)
        // Or the more complex: a ^ b = (a + b) - 2*(a & b)

        std::string key_str = std::to_string(xor_key);
        std::string or_var = "%" + nextTemp();
        std::string and_var = "%" + nextTemp();

        // (idx | key) - (idx & key)
        code.push_back("  " + or_var + " = or i64 " + idx_var + ", " + key_str);
        code.push_back("  " + and_var + " = and i64 " + idx_var + ", " + key_str);
        code.push_back("  " + result_var + " = sub i64 " + or_var + ", " + and_var);

        return code;
    }

    /**
     * Transform unconditional branch
     */
    std::vector<std::string> transformUnconditionalBranch(
        const BranchInfo& branch,
        const JumpTable& table,
        const IndirectBranchConfig& config) {

        std::vector<std::string> code;

        // For unconditional branch, index is always 0
        std::string idx_var = "%" + nextTemp();
        std::string obf_idx_var = "%" + nextTemp();
        std::string ptr_var = "%" + nextTemp();
        std::string target_var = "%" + nextTemp();

        // Get the obfuscated index that maps to original index 0
        const JumpTableEntry* entry = table.getEntryByOriginal(0);
        if (!entry) {
            // Fallback to original branch
            code.push_back(branch.original_instruction);
            return code;
        }

        // Compute the original index (0) in a non-obvious way
        // We store the obfuscated value and reverse it
        code.push_back("  ; Indirect branch (unconditional)");

        // Create the "encrypted" index value
        std::string const_obf_idx = std::to_string(entry->obfuscated_index);

        // Load from jump table
        code.push_back("  " + ptr_var + " = getelementptr inbounds [" +
            std::to_string(table.table_size) + " x i8*], [" +
            std::to_string(table.table_size) + " x i8*]* @" +
            table.table_name + ", i64 0, i64 " + const_obf_idx);
        code.push_back("  " + target_var + " = load i8*, i8** " + ptr_var);

        // Generate indirectbr with all possible targets
        std::string targets_list;
        for (const auto& e : table.entries) {
            if (!e.is_decoy) {
                if (!targets_list.empty()) targets_list += ", ";
                targets_list += "label %" + e.target_label;
            }
        }
        code.push_back("  indirectbr i8* " + target_var + ", [" + targets_list + "]");

        return code;
    }

    /**
     * Transform conditional branch
     */
    std::vector<std::string> transformConditionalBranch(
        const BranchInfo& branch,
        const JumpTable& table,
        const IndirectBranchConfig& config) {

        std::vector<std::string> code;

        code.push_back("  ; Indirect branch (conditional)");

        // Convert condition to index: true=0, false=1
        std::string idx_var = "%" + nextTemp();
        std::string idx_ext = "%" + nextTemp();
        std::string obf_idx_var = "%" + nextTemp();
        std::string ptr_var = "%" + nextTemp();
        std::string target_var = "%" + nextTemp();

        // zext i1 to i64: true becomes 0... wait, we need true=0, false=1
        // So we negate: NOT(cond) => !true=0 becomes 1, !false=1 becomes 0
        // Actually simpler: select i1 %cond, i64 0, i64 1
        code.push_back("  " + idx_var + " = select i1 " + branch.condition +
            ", i64 0, i64 1");

        // Get obfuscated indices
        const JumpTableEntry* true_entry = table.getEntryByOriginal(0);
        const JumpTableEntry* false_entry = table.getEntryByOriginal(1);

        if (!true_entry || !false_entry) {
            // Fallback
            code.push_back(branch.original_instruction);
            return code;
        }

        // Apply obfuscation to index
        auto idx_code = generateIndexCalculation(table, idx_var, obf_idx_var, config);
        code.insert(code.end(), idx_code.begin(), idx_code.end());

        // Load from jump table
        code.push_back("  " + ptr_var + " = getelementptr inbounds [" +
            std::to_string(table.table_size) + " x i8*], [" +
            std::to_string(table.table_size) + " x i8*]* @" +
            table.table_name + ", i64 0, i64 " + obf_idx_var);
        code.push_back("  " + target_var + " = load i8*, i8** " + ptr_var);

        // Generate indirectbr
        std::string targets_list;
        std::vector<std::string> unique_targets;
        for (const auto& e : table.entries) {
            if (!e.is_decoy) {
                if (std::find(unique_targets.begin(), unique_targets.end(),
                    e.target_label) == unique_targets.end()) {
                    unique_targets.push_back(e.target_label);
                }
            }
        }
        for (const auto& t : unique_targets) {
            if (!targets_list.empty()) targets_list += ", ";
            targets_list += "label %" + t;
        }
        code.push_back("  indirectbr i8* " + target_var + ", [" + targets_list + "]");

        return code;
    }

    /**
     * Transform switch statement
     */
    std::vector<std::string> transformSwitch(
        const BranchInfo& branch,
        const JumpTable& table,
        const IndirectBranchConfig& config) {

        std::vector<std::string> code;

        code.push_back("  ; Indirect branch (switch)");

        // For switch, we need to map case values to table indices
        // This is more complex - we might need a secondary lookup
        // For now, use the switch value directly as base for index

        std::string idx_var = "%" + nextTemp();
        std::string obf_idx_var = "%" + nextTemp();
        std::string ptr_var = "%" + nextTemp();
        std::string target_var = "%" + nextTemp();

        // Extend switch value to i64
        code.push_back("  " + idx_var + " = sext i32 " + branch.condition + " to i64");

        // Apply obfuscation
        auto idx_code = generateIndexCalculation(table, idx_var, obf_idx_var, config);
        code.insert(code.end(), idx_code.begin(), idx_code.end());

        // Bounds check for safety (go to default if out of bounds)
        std::string bounds_var = "%" + nextTemp();
        std::string safe_idx = "%" + nextTemp();
        code.push_back("  " + bounds_var + " = icmp ult i64 " + obf_idx_var +
            ", " + std::to_string(table.table_size));
        code.push_back("  " + safe_idx + " = select i1 " + bounds_var +
            ", i64 " + obf_idx_var + ", i64 0");

        // Load from jump table
        code.push_back("  " + ptr_var + " = getelementptr inbounds [" +
            std::to_string(table.table_size) + " x i8*], [" +
            std::to_string(table.table_size) + " x i8*]* @" +
            table.table_name + ", i64 0, i64 " + safe_idx);
        code.push_back("  " + target_var + " = load i8*, i8** " + ptr_var);

        // Generate indirectbr with all unique targets
        std::vector<std::string> unique_targets;
        for (const auto& e : table.entries) {
            if (std::find(unique_targets.begin(), unique_targets.end(),
                e.target_label) == unique_targets.end()) {
                unique_targets.push_back(e.target_label);
            }
        }
        std::string targets_list;
        for (const auto& t : unique_targets) {
            if (!targets_list.empty()) targets_list += ", ";
            targets_list += "label %" + t;
        }
        code.push_back("  indirectbr i8* " + target_var + ", [" + targets_list + "]");

        return code;
    }

    /**
     * Insert jump table declarations at module level
     */
    void insertJumpTableDeclarations(IndirectBranchResult& result) {
        std::vector<std::string> declarations;

        for (const auto& table : result.tables) {
            // Build table entries
            std::string entries;
            for (size_t i = 0; i < table.entries.size(); i++) {
                if (i > 0) entries += ", ";
                // Find entry at this position
                for (const auto& entry : table.entries) {
                    if (static_cast<size_t>(entry.obfuscated_index) == i ||
                        (entry.is_decoy && i >= table.real_entries)) {
                        entries += "i8* blockaddress(@_func_, %" + entry.target_label + ")";
                        break;
                    }
                }
            }

            // Note: In real LLVM IR, blockaddress requires the function name
            // This is a simplified version - actual implementation would track function context
            declarations.push_back("; Jump table: " + table.table_name);
            declarations.push_back("@" + table.table_name + " = private unnamed_addr constant [" +
                std::to_string(table.table_size) + " x i8*] [" + entries + "]");
        }

        // Insert declarations near the top of the module
        // Find a suitable insertion point (after module-level declarations)
        size_t insert_pos = 0;
        for (size_t i = 0; i < result.transformed_code.size(); i++) {
            const std::string& line = result.transformed_code[i];
            // Insert before first function definition
            if (line.find("define ") != std::string::npos) {
                insert_pos = i;
                break;
            }
        }

        result.transformed_code.insert(
            result.transformed_code.begin() + insert_pos,
            declarations.begin(), declarations.end());
    }
};

/**
 * LLVM IR Indirect Branch Pass
 */
class LLVMIndirectBranchPass : public LLVMTransformationPass {
public:
    LLVMIndirectBranchPass() : analyzer_(), transformer_() {}

    std::string getName() const override { return "IndirectBranch"; }
    std::string getDescription() const override {
        return "Converts direct branches to indirect jumps via lookup tables";
    }

    PassPriority getPriority() const override { return PassPriority::ControlFlow; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        ib_config_.enabled = config.enabled;
        ib_config_.probability = config.probability;
        return true;
    }

    void setIndirectBranchConfig(const IndirectBranchConfig& config) {
        ib_config_ = config;
    }

    const IndirectBranchConfig& getConfig() const {
        return ib_config_;
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!ib_config_.enabled) {
            return TransformResult::Skipped;
        }

        // Find all branches
        auto branches = analyzer_.findBranches(lines);
        statistics_["branches_found"] = static_cast<int>(branches.size());

        // Transform branches
        auto ib_result = transformer_.transform(lines, branches, ib_config_);

        if (ib_result.success) {
            lines = std::move(ib_result.transformed_code);
            statistics_["branches_transformed"] = ib_result.branches_transformed;
            statistics_["tables_created"] = ib_result.tables_created;
            statistics_["decoy_entries"] = ib_result.decoy_entries_added;
            return TransformResult::Success;
        } else {
            return TransformResult::Error;
        }
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    LLVMBranchAnalyzer analyzer_;
    LLVMIndirectBranchTransformation transformer_;
    IndirectBranchConfig ib_config_;
};

} // namespace control_flow
} // namespace morphect

#endif // MORPHECT_INDIRECT_BRANCH_HPP
