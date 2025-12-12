/**
 * Morphect - LLVM IR CFG Analyzer
 *
 * Analyzes LLVM IR to build a control flow graph representation.
 */

#include "cff_base.hpp"
#include <regex>
#include <algorithm>

namespace morphect {
namespace cff {

std::optional<CFGInfo> LLVMCFGAnalyzer::analyze(const std::vector<std::string>& lines) {
    CFGInfo cfg;

    // Find function start
    size_t func_start = 0;
    bool found_func = false;

    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].find("define ") != std::string::npos &&
            lines[i].find("{") != std::string::npos) {
            func_start = i;
            found_func = true;

            // Extract function name
            std::regex name_pattern(R"(define\s+\S+\s+@([\w.]+)\s*\()");
            std::smatch match;
            if (std::regex_search(lines[i], match, name_pattern)) {
                cfg.function_name = match[1].str();
            }
            break;
        }
    }

    if (!found_func) {
        return std::nullopt;
    }

    // Parse basic blocks
    std::string current_label = "entry";
    BasicBlockInfo current_block;
    current_block.id = 0;
    current_block.label = current_label;
    current_block.is_entry = true;

    std::unordered_map<std::string, int> label_to_id;
    label_to_id["entry"] = 0;
    int next_id = 1;

    for (size_t i = func_start + 1; i < lines.size(); i++) {
        const std::string& line = lines[i];

        // End of function
        if (line.find("}") != std::string::npos && line.find("{") == std::string::npos) {
            // Save last block
            if (!current_block.code.empty() || !current_block.terminator.empty()) {
                cfg.blocks.push_back(current_block);
            }
            break;
        }

        // Skip empty lines and comments
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start);

        if (trimmed.empty() || trimmed[0] == ';') continue;

        // Check for label (new basic block)
        std::regex label_pattern(R"(^([\w.]+):)");
        std::smatch match;
        if (std::regex_search(trimmed, match, label_pattern)) {
            // Save previous block
            if (!current_block.code.empty() || !current_block.terminator.empty()) {
                cfg.blocks.push_back(current_block);
            }

            // Start new block
            current_label = match[1].str();
            if (label_to_id.find(current_label) == label_to_id.end()) {
                label_to_id[current_label] = next_id++;
            }

            current_block = BasicBlockInfo();
            current_block.id = label_to_id[current_label];
            current_block.label = current_label;
            continue;
        }

        // Check for terminator
        if (trimmed.find("ret ") == 0 || trimmed.find("ret\n") == 0) {
            current_block.terminator = trimmed;
            current_block.is_exit = true;
            cfg.exit_blocks.push_back(current_block.id);
        }
        else if (trimmed.find("br ") == 0) {
            current_block.terminator = trimmed;
            parseTerminator(current_block, trimmed);
        }
        else if (trimmed.find("switch ") == 0) {
            // Switch can span multiple lines - collect all of it
            std::string switch_terminator = trimmed;
            // Check if switch continues on next lines (look for closing ])
            while (switch_terminator.find(']') == std::string::npos && i + 1 < lines.size()) {
                i++;
                std::string next_line = lines[i];
                size_t next_start = next_line.find_first_not_of(" \t");
                if (next_start != std::string::npos) {
                    switch_terminator += "\n" + next_line.substr(next_start);
                }
            }
            current_block.terminator = switch_terminator;
            current_block.has_switch = true;

            // Parse switch: switch i32 %val, label %default [ i32 0, label %case0 ... ]
            // Extract the switch condition variable
            std::regex switch_cond_pattern(R"(switch\s+\w+\s+(%[\w.]+))");
            std::smatch cond_match;
            if (std::regex_search(switch_terminator, cond_match, switch_cond_pattern)) {
                current_block.switch_condition = cond_match[1].str();
            }

            // Extract default and all case targets
            std::regex default_pattern(R"(switch\s+\w+\s+[^,]+,\s*label\s+%([\w.]+))");
            std::smatch match;
            if (std::regex_search(switch_terminator, match, default_pattern)) {
                std::string default_label = match[1].str();
                if (label_to_id.find(default_label) == label_to_id.end()) {
                    label_to_id[default_label] = next_id++;
                }
                current_block.switch_default = label_to_id[default_label];
            }

            // Extract all case labels
            std::regex case_pattern(R"(i32\s+(-?\d+),\s*label\s+%([\w.]+))");
            auto case_begin = std::sregex_iterator(switch_terminator.begin(), switch_terminator.end(), case_pattern);
            auto case_end = std::sregex_iterator();
            for (std::sregex_iterator it = case_begin; it != case_end; ++it) {
                std::smatch case_match = *it;
                int case_val = std::stoi(case_match[1].str());
                std::string case_label = case_match[2].str();
                if (label_to_id.find(case_label) == label_to_id.end()) {
                    label_to_id[case_label] = next_id++;
                }
                current_block.switch_cases.push_back({case_val, label_to_id[case_label]});
            }
        }
        else if (trimmed.find("unreachable") == 0) {
            current_block.terminator = trimmed;
            current_block.is_exit = true;
        }
        else if (trimmed.find("invoke ") != std::string::npos) {
            // Invoke is like call + branch to normal/exception
            // Format: %result = invoke rettype @func(args) to label %normal unwind label %exception
            current_block.terminator = trimmed;
            current_block.has_invoke = true;
            current_block.invoke_instruction = trimmed;
            cfg.has_exception_handling = true;
            cfg.invoke_blocks.push_back(current_block.id);

            // Parse invoke destinations
            std::regex invoke_pattern(R"(to\s+label\s+%([\w.]+)\s+unwind\s+label\s+%([\w.]+))");
            std::smatch match;
            if (std::regex_search(trimmed, match, invoke_pattern)) {
                std::string normal_label = match[1].str();
                std::string unwind_label = match[2].str();

                // Ensure labels have IDs
                if (label_to_id.find(normal_label) == label_to_id.end()) {
                    label_to_id[normal_label] = next_id++;
                }
                if (label_to_id.find(unwind_label) == label_to_id.end()) {
                    label_to_id[unwind_label] = next_id++;
                }

                current_block.normal_dest = label_to_id[normal_label];
                current_block.unwind_dest = label_to_id[unwind_label];
            }
        }
        else if (trimmed.find("resume ") == 0) {
            // Resume re-throws the exception
            current_block.terminator = trimmed;
            current_block.has_resume = true;
            current_block.is_exit = true;  // Effectively exits the function
            cfg.exit_blocks.push_back(current_block.id);
        }
        else if (trimmed.find("landingpad ") != std::string::npos) {
            // Landing pad is the first instruction after exception
            // Keep it in code but mark the block
            current_block.is_landing_pad = true;
            current_block.code.push_back(line);
            cfg.has_exception_handling = true;
            cfg.landing_pads.push_back(current_block.id);
        }
        else if (trimmed.find("cleanupret ") == 0 || trimmed.find("catchret ") == 0) {
            // Cleanup/catch return terminators
            current_block.terminator = trimmed;
            // These have specific successors but we treat them as exit-like
        }
        else {
            // Regular instruction
            current_block.code.push_back(line);
        }
    }

    // Build successor/predecessor relationships
    for (auto& block : cfg.blocks) {
        if (block.has_invoke) {
            // Invoke has two successors: normal and unwind
            if (block.normal_dest >= 0) {
                block.successors.push_back(block.normal_dest);
            }
            if (block.unwind_dest >= 0) {
                block.successors.push_back(block.unwind_dest);
            }
        }
        else if (block.terminator.find("br i1") != std::string::npos) {
            // Conditional branch: br i1 %cond, label %true, label %false
            std::regex cond_br(R"(br\s+i1\s+(%[\w.]+),\s*label\s+%([\w.]+),\s*label\s+%([\w.]+))");
            std::smatch match;
            if (std::regex_search(block.terminator, match, cond_br)) {
                std::string true_label = match[2].str();
                std::string false_label = match[3].str();

                if (label_to_id.find(true_label) != label_to_id.end()) {
                    block.true_target = label_to_id[true_label];
                    block.successors.push_back(block.true_target);
                }
                if (label_to_id.find(false_label) != label_to_id.end()) {
                    block.false_target = label_to_id[false_label];
                    block.successors.push_back(block.false_target);
                }
                block.condition = match[1].str();
                block.has_conditional = true;
            }
        }
        else if (block.terminator.find("br label") != std::string::npos) {
            // Unconditional branch: br label %target
            std::regex uncond_br(R"(br\s+label\s+%([\w.]+))");
            std::smatch match;
            if (std::regex_search(block.terminator, match, uncond_br)) {
                std::string target_label = match[1].str();
                if (label_to_id.find(target_label) != label_to_id.end()) {
                    int target_id = label_to_id[target_label];
                    block.successors.push_back(target_id);
                    block.true_target = target_id;
                }
            }
        }
        else if (block.has_switch) {
            // Switch: add default and all case targets as successors
            if (block.switch_default >= 0) {
                block.successors.push_back(block.switch_default);
            }
            for (const auto& [case_val, target_id] : block.switch_cases) {
                // Avoid duplicates
                if (std::find(block.successors.begin(), block.successors.end(), target_id) == block.successors.end()) {
                    block.successors.push_back(target_id);
                }
            }
        }
    }

    // Build predecessor list
    for (auto& block : cfg.blocks) {
        for (int succ : block.successors) {
            for (auto& target : cfg.blocks) {
                if (target.id == succ) {
                    target.predecessors.push_back(block.id);
                    break;
                }
            }
        }
    }

    // Set entry block
    cfg.entry_block = 0;  // First block is always entry

    // Compute statistics
    cfg.num_blocks = static_cast<int>(cfg.blocks.size());
    for (const auto& block : cfg.blocks) {
        cfg.num_edges += static_cast<int>(block.successors.size());
        if (block.has_conditional) cfg.num_conditionals++;
    }

    // Identify loops
    identifyLoops(cfg);

    return cfg;
}

void LLVMCFGAnalyzer::parseTerminator(BasicBlockInfo& block, const std::string& line) {
    // Parse conditional branch
    std::regex cond_br(R"(br\s+i1\s+(%[\w.]+),\s*label\s+%([\w.]+),\s*label\s+%([\w.]+))");
    std::smatch match;
    if (std::regex_search(line, match, cond_br)) {
        block.condition = match[1].str();
        block.has_conditional = true;
        return;
    }

    // Parse unconditional branch
    std::regex uncond_br(R"(br\s+label\s+%([\w.]+))");
    if (std::regex_search(line, match, uncond_br)) {
        block.has_conditional = false;
        return;
    }
}

void LLVMCFGAnalyzer::identifyLoops(CFGInfo& cfg) {
    // Find back edges using DFS
    std::unordered_set<int> visited;
    std::unordered_set<int> in_stack;

    dfsLoops(cfg, cfg.entry_block, visited, in_stack);

    // Mark loop headers
    for (const auto& edge : cfg.back_edges) {
        cfg.loop_headers.insert(edge.second);
        cfg.num_loops++;

        // Mark the header block
        auto* header = cfg.getBlock(edge.second);
        if (header) {
            header->is_loop_header = true;
        }

        // Mark the latch block
        auto* latch = cfg.getBlock(edge.first);
        if (latch) {
            latch->is_loop_latch = true;
        }
    }
}

void LLVMCFGAnalyzer::dfsLoops(CFGInfo& cfg, int node,
                                std::unordered_set<int>& visited,
                                std::unordered_set<int>& in_stack) {
    visited.insert(node);
    in_stack.insert(node);

    auto* block = cfg.getBlock(node);
    if (!block) return;

    for (int succ : block->successors) {
        if (in_stack.find(succ) != in_stack.end()) {
            // Back edge found!
            cfg.back_edges.push_back({node, succ});
        }
        else if (visited.find(succ) == visited.end()) {
            dfsLoops(cfg, succ, visited, in_stack);
        }
    }

    in_stack.erase(node);
}

} // namespace cff
} // namespace morphect
