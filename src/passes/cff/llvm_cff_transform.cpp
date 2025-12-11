/**
 * Morphect - LLVM IR Control Flow Flattening Transformation
 *
 * Transforms a function's CFG into a switch-based state machine.
 */

#include "cff_base.hpp"
#include <sstream>
#include <algorithm>
#include <regex>

namespace morphect {
namespace cff {

CFFResult LLVMCFFTransformation::flatten(const CFGInfo& cfg, const CFFConfig& config) {
    CFFResult result;
    temp_counter_ = 0;
    phi_nodes_.clear();
    phi_alloca_map_.clear();
    phi_replacement_map_.clear();
    entry_allocas_.clear();  // Clear entry block allocas
    has_return_value_ = false;
    return_type_ = "void";
    return_alloca_ = "%_cff_retval";

    // Validate
    if (cfg.blocks.empty()) {
        result.error = "No blocks to flatten";
        return result;
    }

    if (cfg.blocks.size() < 2) {
        result.error = "Need at least 2 blocks for flattening";
        return result;
    }

    // Check for exception handling - skip functions with invoke/landingpad
    // Exception handling has strict structural requirements that conflict with CFF
    if (cfg.has_exception_handling) {
        logger_.debug("Function has exception handling - skipping CFF (invoke/landingpad not supported)");
        result.error = "Function has exception handling (invoke/landingpad) - cannot flatten";
        return result;
    }

    result.original_blocks = static_cast<int>(cfg.blocks.size());

    // First pass: identify all PHI nodes and return type
    collectPhiNodes(cfg);

    // Determine return type from exit blocks
    for (const auto& block : cfg.blocks) {
        if (block.is_exit && block.terminator.find("ret ") != std::string::npos) {
            // Extract return type from terminator: "ret i32 %val" or "ret void"
            std::regex ret_pattern(R"(ret\s+(\w+)(\s+.+)?)");
            std::smatch match;
            if (std::regex_search(block.terminator, match, ret_pattern)) {
                return_type_ = match[1].str();
                if (return_type_ != "void") {
                    has_return_value_ = true;
                }
            }
            break;
        }
    }

    // Extract allocas from the original entry block - they must be in the new entry
    // block to dominate all uses across all states
    for (const auto& block : cfg.blocks) {
        if (block.is_entry) {
            for (const auto& line : block.code) {
                // Match alloca instructions
                if (line.find(" = alloca ") != std::string::npos) {
                    entry_allocas_.push_back(line);
                }
            }
            break;
        }
    }

    // Assign state values
    auto states = assignStates(cfg, config);
    int end_state = static_cast<int>(cfg.blocks.size());  // END_STATE
    result.states_created = end_state + 1;

    // Generate the flattened code
    std::vector<std::string> output;

    // Function header (preserved from original)
    output.push_back("; Flattened function: " + cfg.function_name);

    // Entry block: initialize state and jump to dispatcher
    output.push_back("entry_flat:");

    // First, add allocas from the original entry block (they must dominate all uses)
    for (const auto& alloca_line : entry_allocas_) {
        output.push_back(alloca_line);
    }

    // State variable alloca
    output.push_back("  %" + config.state_var_name + " = alloca i32");
    output.push_back("  store i32 0, i32* %" + config.state_var_name);

    // Return value alloca (if function returns a value)
    if (has_return_value_) {
        output.push_back("  " + return_alloca_ + " = alloca " + return_type_);
    }

    // Generate allocas for PHI variables
    auto phi_allocas = generatePhiHandling(cfg, states);
    output.insert(output.end(), phi_allocas.begin(), phi_allocas.end());

    output.push_back("  br label %dispatcher");
    output.push_back("");

    // Dispatcher block
    output.push_back("dispatcher:");
    output.push_back("  %" + config.state_var_name + "_val = load i32, i32* %" + config.state_var_name);

    // Build switch instruction
    std::stringstream switch_ss;
    switch_ss << "  switch i32 %" << config.state_var_name << "_val, label %end_state [";

    // Add cases for each state
    for (const auto& block : cfg.blocks) {
        int state = states.at(block.id);
        switch_ss << "\n    i32 " << state << ", label %state_" << state;
    }
    switch_ss << "\n  ]";
    output.push_back(switch_ss.str());
    output.push_back("");

    // Generate state blocks
    for (const auto& block : cfg.blocks) {
        auto case_code = generateCase(block, states, end_state, config);
        output.insert(output.end(), case_code.begin(), case_code.end());
    }

    // End state (exit)
    output.push_back("end_state:");
    output.push_back("  ; Function completed");

    // Return the appropriate value
    if (return_type_ == "void") {
        output.push_back("  ret void");
    } else if (has_return_value_) {
        // Load the stored return value and return it
        std::string ret_loaded = nextTemp();
        output.push_back("  " + ret_loaded + " = load " + return_type_ +
                        ", " + return_type_ + "* " + return_alloca_);
        output.push_back("  ret " + return_type_ + " " + ret_loaded);
    } else {
        // Fallback: return default value (shouldn't happen if analysis is correct)
        output.push_back("  ret " + return_type_ + " 0  ; fallback");
    }

    result.transformed_code = output;
    result.flattened_blocks = static_cast<int>(cfg.blocks.size()) + 2;  // + dispatcher + end
    result.success = true;

    return result;
}

void LLVMCFFTransformation::collectPhiNodes(const CFGInfo& cfg) {
    phi_nodes_.clear();

    for (const auto& block : cfg.blocks) {
        for (const auto& line : block.code) {
            // Check for PHI instruction: %result = phi type [ val1, %label1 ], [ val2, %label2 ]
            std::regex phi_pattern(R"(\s*(%[\w.]+)\s*=\s*phi\s+(\w+)\s+(.+))");
            std::smatch match;
            if (std::regex_search(line, match, phi_pattern)) {
                PhiNodeInfo phi;
                phi.result = match[1].str();
                phi.type = match[2].str();
                phi.block_id = block.id;

                // Parse incoming values: [ val, %label ], ...
                std::string args = match[3].str();
                std::regex arg_pattern(R"(\[\s*([^,]+),\s*%([\w.]+)\s*\])");
                auto args_begin = std::sregex_iterator(args.begin(), args.end(), arg_pattern);
                auto args_end = std::sregex_iterator();

                for (std::sregex_iterator i = args_begin; i != args_end; ++i) {
                    std::smatch arg_match = *i;
                    phi.incoming_values.push_back(arg_match[1].str());
                    phi.incoming_labels.push_back(arg_match[2].str());
                }

                phi_nodes_.push_back(phi);
            }
        }
    }
}

std::vector<std::string> LLVMCFFTransformation::generateCase(
    const BasicBlockInfo& block,
    const std::unordered_map<int, int>& states,
    int end_state,
    const CFFConfig& config) {

    std::vector<std::string> output;
    int state = states.at(block.id);

    output.push_back("state_" + std::to_string(state) + ":  ; original: " + block.label);

    // Special handling for landing pad blocks - they must keep their landing pad instruction
    if (block.is_landing_pad) {
        output.push_back("  ; Landing pad block - exception handling preserved");
    }

    // Generate PHI loads at the start of this block (for PHIs defined here)
    auto phi_loads = generatePhiLoads(block);
    output.insert(output.end(), phi_loads.begin(), phi_loads.end());

    // Copy original block code (without terminator, skip PHI nodes, and skip allocas for entry block)
    for (const auto& line : block.code) {
        // Skip PHI instructions (they're handled separately)
        if (line.find(" phi ") != std::string::npos) {
            continue;
        }

        // Skip alloca instructions for entry block (they were moved to entry_flat)
        if (block.is_entry && line.find(" = alloca ") != std::string::npos) {
            continue;
        }

        // Replace PHI variable references with loaded values
        std::string modified_line = line;
        for (const auto& [phi_result, loaded_var] : phi_replacement_map_) {
            size_t pos = 0;
            while ((pos = modified_line.find(phi_result, pos)) != std::string::npos) {
                modified_line.replace(pos, phi_result.length(), loaded_var);
                pos += loaded_var.length();
            }
        }
        output.push_back(modified_line);
    }

    // Generate PHI stores before state update (for successor blocks)
    auto phi_stores = generatePhiStores(block, states);
    output.insert(output.end(), phi_stores.begin(), phi_stores.end());

    // Special handling for invoke blocks
    if (block.has_invoke) {
        // For invoke instructions, we need to preserve the exception handling semantics
        // Strategy: Convert invoke to a call wrapped in exception handling
        // The state update will handle the normal path

        output.push_back("  ; Invoke block - exception handling preserved");

        // Keep the original invoke instruction but update the labels
        // The invoke will jump to the landing pad on exception, which is also in the dispatcher
        std::string modified_invoke = block.invoke_instruction;

        // Update destination labels to state_N format
        if (block.normal_dest >= 0 && states.find(block.normal_dest) != states.end()) {
            int normal_state = states.at(block.normal_dest);
            // Note: For proper exception handling, we need to keep the invoke as-is
            // The flattened version must preserve invoke semantics
            output.push_back("  ; Normal dest state: " + std::to_string(normal_state));
        }
        if (block.unwind_dest >= 0 && states.find(block.unwind_dest) != states.end()) {
            int unwind_state = states.at(block.unwind_dest);
            output.push_back("  ; Unwind dest state: " + std::to_string(unwind_state));
        }
    }

    // Generate state update based on terminator
    auto state_update = generateStateUpdate(block, states, end_state, config);
    output.insert(output.end(), state_update.begin(), state_update.end());

    // Handle resume blocks specially - they exit via exception
    if (block.has_resume) {
        // Resume re-throws the exception - it doesn't go back to dispatcher
        output.push_back("  ; Resume - exception propagation");
        output.push_back("  " + block.terminator);  // Keep the original resume
    } else {
        // Jump back to dispatcher
        output.push_back("  br label %dispatcher");
    }
    output.push_back("");

    return output;
}

std::vector<std::string> LLVMCFFTransformation::generateStateUpdate(
    const BasicBlockInfo& block,
    const std::unordered_map<int, int>& states,
    int end_state,
    const CFFConfig& config) {

    std::vector<std::string> output;

    if (block.is_exit) {
        // Exit block - set state to END_STATE
        output.push_back("  store i32 " + std::to_string(end_state) +
                        ", i32* %" + config.state_var_name + "  ; exit");

        // Handle return value if needed - store it to the return alloca
        if (block.terminator.find("ret ") != std::string::npos &&
            block.terminator.find("ret void") == std::string::npos) {
            // Extract return type and value from terminator: "ret i32 %val" or "ret i32 0"
            std::regex ret_val(R"(ret\s+(\w+)\s+(.+))");
            std::smatch match;
            if (std::regex_search(block.terminator, match, ret_val)) {
                std::string ret_type = match[1].str();
                std::string ret_value = match[2].str();
                // Store the return value for retrieval at end_state
                output.push_back("  store " + ret_type + " " + ret_value +
                                ", " + ret_type + "* " + return_alloca_);
            }
        }

        // Handle resume (re-throwing exception)
        if (block.has_resume) {
            output.push_back("  ; Exception re-thrown via resume");
        }
    }
    else if (block.has_invoke) {
        // Invoke instruction: has two possible destinations (normal and unwind)
        // For flattening, we need to convert the invoke to a call + conditional state update
        // The actual invoke will be replaced in generateCase

        // For state machine: normal path goes to normal_dest, exception path to unwind_dest
        // Since exceptions are runtime events, we only set the normal path state here
        // Landing pads handle the exception case
        if (block.normal_dest >= 0 && states.find(block.normal_dest) != states.end()) {
            int normal_state = states.at(block.normal_dest);
            output.push_back("  ; Invoke - setting state for normal path");
            output.push_back("  store i32 " + std::to_string(normal_state) +
                            ", i32* %" + config.state_var_name);
        }

        // Note: The unwind_dest is still valid as exception handling is preserved
        // The invoke instruction itself remains and handles the exception flow
    }
    else if (block.has_conditional) {
        // Conditional branch - compute next state based on condition
        int true_state = states.at(block.true_target);
        int false_state = states.at(block.false_target);

        std::string tmp = nextTemp();
        output.push_back("  " + tmp + " = select i1 " + block.condition +
                        ", i32 " + std::to_string(true_state) +
                        ", i32 " + std::to_string(false_state));
        output.push_back("  store i32 " + tmp + ", i32* %" + config.state_var_name);
    }
    else if (!block.successors.empty()) {
        // Unconditional branch
        int next_state = states.at(block.successors[0]);
        output.push_back("  store i32 " + std::to_string(next_state) +
                        ", i32* %" + config.state_var_name);
    }
    else {
        // No successors - treat as implicit exit
        output.push_back("  store i32 " + std::to_string(end_state) +
                        ", i32* %" + config.state_var_name + "  ; implicit exit");
    }

    return output;
}

std::vector<std::string> LLVMCFFTransformation::generateDispatcher(
    const CFGInfo& cfg,
    const std::unordered_map<int, int>& states,
    const CFFConfig& config) {

    std::vector<std::string> output;

    // This is called from flatten() but the main logic is there
    // This method could be used for more complex dispatcher generation

    return output;
}

std::vector<std::string> LLVMCFFTransformation::generatePhiHandling(
    const CFGInfo& cfg,
    const std::unordered_map<int, int>& states) {

    std::vector<std::string> output;

    // PHI nodes need special handling in CFF
    // When flattening, all paths go through the dispatcher
    // PHI nodes need to be converted to loads from stored values

    if (phi_nodes_.empty()) {
        return output;
    }

    // Create alloca for each PHI value (at function entry)
    output.push_back("; PHI node variables");
    for (size_t i = 0; i < phi_nodes_.size(); i++) {
        const auto& phi = phi_nodes_[i];
        std::string var_name = "%phi_var_" + std::to_string(i);
        output.push_back("  " + var_name + " = alloca " + phi.type);
        phi_alloca_map_[phi.result] = var_name;
    }
    output.push_back("");

    return output;
}

std::vector<std::string> LLVMCFFTransformation::generatePhiStores(
    const BasicBlockInfo& block,
    const std::unordered_map<int, int>& states) {

    std::vector<std::string> output;

    // For each PHI node that has this block as a predecessor,
    // generate a store of the incoming value

    for (const auto& [phi_result, alloca_var] : phi_alloca_map_) {
        // Find the PHI node info
        for (const auto& phi : phi_nodes_) {
            if (phi.result == phi_result) {
                // Check if current block is a predecessor
                for (size_t i = 0; i < phi.incoming_labels.size(); i++) {
                    if (phi.incoming_labels[i] == block.label) {
                        // Store the incoming value
                        std::string type = phi.type;
                        std::string value = phi.incoming_values[i];
                        output.push_back("  store " + type + " " + value +
                                        ", " + type + "* " + alloca_var);
                        break;
                    }
                }
            }
        }
    }

    return output;
}

std::vector<std::string> LLVMCFFTransformation::generatePhiLoads(
    const BasicBlockInfo& block) {

    std::vector<std::string> output;

    // For each PHI node in this block, generate a load
    for (const auto& phi : phi_nodes_) {
        if (phi.block_id == block.id) {
            auto it = phi_alloca_map_.find(phi.result);
            if (it != phi_alloca_map_.end()) {
                std::string loaded = phi.result + "_loaded";
                output.push_back("  " + loaded + " = load " + phi.type +
                                ", " + phi.type + "* " + it->second);
                // Note: Uses of phi.result should be replaced with loaded
                phi_replacement_map_[phi.result] = loaded;
            }
        }
    }

    return output;
}

// ============================================================================
// LLVMCFFPass Implementation
// ============================================================================

TransformResult LLVMCFFPass::transformIR(std::vector<std::string>& lines) {
    if (!cff_config_.enabled) {
        return TransformResult::Skipped;
    }

    // Find all functions
    auto functions = findFunctions(lines);

    if (functions.empty()) {
        return TransformResult::NotApplicable;
    }

    int transformed = 0;
    std::vector<std::string> new_lines;
    size_t last_end = 0;

    for (const auto& [start, end] : functions) {
        // Copy lines before this function
        for (size_t i = last_end; i < start; i++) {
            new_lines.push_back(lines[i]);
        }

        // Extract function
        auto func_lines = extractFunction(lines, start, end);

        // Analyze CFG
        auto cfg_opt = analyzer_.analyze(func_lines);
        if (!cfg_opt.has_value()) {
            // Couldn't analyze - keep original
            for (size_t i = start; i <= end; i++) {
                new_lines.push_back(lines[i]);
            }
            last_end = end + 1;
            continue;
        }

        auto& cfg = cfg_opt.value();

        // Check if suitable and probability
        if (!analyzer_.isSuitable(cfg, cff_config_) ||
            !GlobalRandom::decide(cff_config_.probability)) {
            // Keep original
            for (size_t i = start; i <= end; i++) {
                new_lines.push_back(lines[i]);
            }
            last_end = end + 1;
            continue;
        }

        // Flatten the function
        auto result = transformer_.flatten(cfg, cff_config_);

        if (result.success) {
            // Extract function signature from original
            new_lines.push_back(lines[start]);  // define line

            // Add flattened code
            for (const auto& line : result.transformed_code) {
                new_lines.push_back(line);
            }

            // Close function
            new_lines.push_back("}");

            transformed++;
            incrementStat("functions_flattened");
            incrementStat("blocks_flattened", result.original_blocks);
        }
        else {
            // Keep original on failure
            for (size_t i = start; i <= end; i++) {
                new_lines.push_back(lines[i]);
            }
        }

        last_end = end + 1;
    }

    // Copy remaining lines
    for (size_t i = last_end; i < lines.size(); i++) {
        new_lines.push_back(lines[i]);
    }

    lines = std::move(new_lines);

    return transformed > 0 ? TransformResult::Success : TransformResult::NotApplicable;
}

std::vector<std::pair<size_t, size_t>> LLVMCFFPass::findFunctions(
    const std::vector<std::string>& lines) {

    std::vector<std::pair<size_t, size_t>> functions;
    size_t func_start = 0;
    bool in_function = false;
    int brace_depth = 0;

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];

        if (!in_function) {
            // Look for function definition
            if (line.find("define ") != std::string::npos) {
                func_start = i;
                in_function = true;
                brace_depth = 0;

                // Count braces on this line
                for (char c : line) {
                    if (c == '{') brace_depth++;
                    if (c == '}') brace_depth--;
                }
            }
        }
        else {
            // Track brace depth
            for (char c : line) {
                if (c == '{') brace_depth++;
                if (c == '}') brace_depth--;
            }

            // Function ends when brace depth returns to 0
            if (brace_depth <= 0) {
                functions.push_back({func_start, i});
                in_function = false;
            }
        }
    }

    return functions;
}

std::vector<std::string> LLVMCFFPass::extractFunction(
    const std::vector<std::string>& lines,
    size_t start, size_t end) {

    std::vector<std::string> func_lines;
    for (size_t i = start; i <= end; i++) {
        func_lines.push_back(lines[i]);
    }
    return func_lines;
}

} // namespace cff
} // namespace morphect
