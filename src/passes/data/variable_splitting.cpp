/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * variable_splitting.cpp - Variable splitting implementation
 */

#include "variable_splitting.hpp"
#include <regex>
#include <sstream>
#include <algorithm>

namespace morphect {
namespace data {

TransformResult LLVMVariableSplittingPass::transformIR(
        std::vector<std::string>& lines) {

    if (!config_.enabled) {
        return TransformResult::Skipped;
    }

    std::vector<std::string> result;
    result.reserve(lines.size() * 2);

    int total_splits = 0;
    bool in_function = false;
    size_t func_start = 0;

    // Process the IR, function by function
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];

        // Detect function start
        if (line.find("define ") != std::string::npos) {
            in_function = true;
            func_start = i;
            result.push_back(line);
            continue;
        }

        // Skip lines inside function body - they'll be processed at function end
        if (in_function && (line.find("}") == std::string::npos ||
            line.find("switch") != std::string::npos)) {
            continue;
        }

        // Detect function end
        if (in_function && line.find("}") != std::string::npos &&
            line.find("switch") == std::string::npos) {
            // End of function - process it
            size_t func_end = i;

            // Transform the function
            auto transformed = transformFunction(lines, func_start, func_end);

            // Add transformed function body
            for (const auto& tline : transformed) {
                result.push_back(tline);
            }

            total_splits += split_vars_.size();
            split_vars_.clear();

            in_function = false;
            result.push_back(line);  // closing brace
            continue;
        }

        result.push_back(line);
    }

    lines = std::move(result);

    // Renumber SSA values to maintain valid LLVM IR
    renumberSSA(lines);

    incrementStat("variables_split", total_splits);

    return total_splits > 0 ? TransformResult::Success : TransformResult::NotApplicable;
}

std::vector<VariableAnalysis> LLVMVariableSplittingPass::analyzeVariables(
        const std::vector<std::string>& lines,
        size_t func_start,
        size_t func_end) {

    std::vector<VariableAnalysis> analyses;
    std::map<std::string, VariableAnalysis> var_map;

    // Pattern for variable definitions: %name = ...
    std::regex def_pattern(R"(^\s*(%[\w.]+)\s*=\s*(\w+)\s+(\w+).*)");

    // Pattern for PHI nodes
    std::regex phi_pattern(R"(^\s*(%[\w.]+)\s*=\s*phi\s+(\w+)\s+.*)");

    for (size_t i = func_start; i < func_end && i < lines.size(); i++) {
        const std::string& line = lines[i];

        std::smatch match;

        // Check for PHI node
        if (std::regex_search(line, match, phi_pattern)) {
            std::string var_name = match[1];
            std::string type = match[2];

            if (var_map.find(var_name) == var_map.end()) {
                VariableAnalysis analysis;
                analysis.name = var_name;
                analysis.type = type;
                analysis.is_phi = true;
                analysis.can_split = config_.split_phi_nodes;
                if (!config_.split_phi_nodes) {
                    analysis.reason_cannot_split = "PHI node splitting disabled";
                }
                var_map[var_name] = analysis;
            }
            var_map[var_name].definition_lines.push_back(static_cast<int>(i));
            continue;
        }

        // Check for regular definition
        if (std::regex_search(line, match, def_pattern)) {
            std::string var_name = match[1];
            std::string op = match[2];
            std::string rest = match[3];

            // Skip certain operations that shouldn't be split
            if (op == "alloca" || op == "load" || op == "getelementptr" ||
                op == "call" || op == "invoke" || op == "landingpad" ||
                op == "extractvalue" || op == "insertvalue") {
                continue;
            }

            // Skip icmp and fcmp - they produce i1 (boolean) and shouldn't be split
            // as arithmetic operations on booleans don't make sense
            if (op == "icmp" || op == "fcmp") {
                continue;
            }

            // Skip select - the type extraction gets the condition type (i1) instead
            // of the result type, which causes issues with split operations
            if (op == "select") {
                continue;
            }

            // Skip type conversion operations - their result type differs from input type
            // and parsing the "to" type is complex
            if (op == "sext" || op == "zext" || op == "trunc" ||
                op == "fpext" || op == "fptrunc" || op == "fptoui" ||
                op == "fptosi" || op == "uitofp" || op == "sitofp" ||
                op == "ptrtoint" || op == "inttoptr" || op == "bitcast" ||
                op == "addrspacecast") {
                continue;
            }

            // Extract actual type, skipping any flags (nsw, nuw, exact, etc.)
            std::string type = rest;
            if (rest == "nsw" || rest == "nuw" || rest == "exact" ||
                rest == "fast" || rest == "nnan" || rest == "ninf" ||
                rest == "nsz" || rest == "arcp" || rest == "contract" ||
                rest == "afn" || rest == "reassoc") {
                // rest is a flag, need to find the actual type in the line
                // Pattern: after the flag, there should be another word (the type)
                std::regex type_after_flag(R"(\s)" + rest + R"(\s+(\w+))");
                std::smatch type_match;
                if (std::regex_search(line, type_match, type_after_flag)) {
                    type = type_match[1];
                }
            }

            if (var_map.find(var_name) == var_map.end()) {
                VariableAnalysis analysis;
                analysis.name = var_name;
                analysis.type = type;
                analysis.is_phi = false;
                analysis.can_split = true;
                var_map[var_name] = analysis;
            }
            var_map[var_name].definition_lines.push_back(static_cast<int>(i));
        }

        // Find uses of variables in the line
        auto uses = findVariableUses(line);
        for (const auto& use : uses) {
            if (var_map.find(use) != var_map.end()) {
                var_map[use].use_lines.push_back(static_cast<int>(i));
            }
        }
    }

    // Convert map to vector
    for (auto& [name, analysis] : var_map) {
        // Check exclusion patterns
        if (isExcluded(name)) {
            analysis.can_split = false;
            analysis.reason_cannot_split = "Matches exclusion pattern";
        }

        // Must have at least one use to be worth splitting
        if (analysis.use_lines.empty()) {
            analysis.can_split = false;
            analysis.reason_cannot_split = "No uses found";
        }

        analyses.push_back(analysis);
    }

    return analyses;
}

std::vector<std::string> LLVMVariableSplittingPass::selectVariablesToSplit(
        const std::vector<VariableAnalysis>& analyses) {

    std::vector<std::string> selected;

    for (const auto& analysis : analyses) {
        if (!analysis.can_split) {
            continue;
        }

        // Apply probability
        if (!GlobalRandom::decide(config_.probability)) {
            continue;
        }

        selected.push_back(analysis.name);

        // Respect max splits limit
        if (static_cast<int>(selected.size()) >= config_.max_splits_per_function) {
            break;
        }
    }

    return selected;
}

SplitVariable LLVMVariableSplittingPass::createSplitVariable(
        const std::string& name,
        const std::string& type) {

    SplitVariable split;
    split.original_name = name;
    split.type = type;

    // Generate unique names for split parts
    std::string base = name.substr(1);  // Remove leading %
    split.part1_name = "%split_" + base + "_p1_" + std::to_string(split_counter_);
    split.part2_name = "%split_" + base + "_p2_" + std::to_string(split_counter_);
    split_counter_++;

    // Choose strategy
    split.strategy = config_.default_strategy;
    split.num_parts = 2;

    return split;
}

std::vector<std::string> LLVMVariableSplittingPass::transformFunction(
        const std::vector<std::string>& lines,
        size_t func_start,
        size_t func_end) {

    std::vector<std::string> result;

    // Analyze variables - only analyze body, not define line or closing brace
    auto analyses = analyzeVariables(lines, func_start + 1, func_end);

    // Select variables to split
    auto to_split = selectVariablesToSplit(analyses);

    if (to_split.empty()) {
        // No variables to split, return only function body (no define, no closing brace)
        for (size_t i = func_start + 1; i < func_end && i < lines.size(); i++) {
            result.push_back(lines[i]);
        }
        return result;
    }

    // Create split variable info
    for (const auto& var_name : to_split) {
        for (const auto& analysis : analyses) {
            if (analysis.name == var_name) {
                split_vars_[var_name] = createSplitVariable(var_name, analysis.type);
                break;
            }
        }
    }

    // Transform lines - only process function body (no define line, no closing brace)
    for (size_t i = func_start + 1; i < func_end && i < lines.size(); i++) {
        const std::string& line = lines[i];

        // Check if this line uses any split variables (in operands, not destination)
        auto uses = findVariableUses(line);
        std::vector<std::string> split_uses;
        for (const auto& use : uses) {
            if (split_vars_.find(use) != split_vars_.end()) {
                split_uses.push_back(use);
            }
        }

        // If there are split variable uses, we need to reconstruct them first
        std::string working_line = line;
        if (!split_uses.empty()) {
            // Generate reconstructions and update the line
            std::map<std::string, std::string> reconstructed;
            for (const auto& use : split_uses) {
                const auto& split_var = split_vars_.at(use);
                std::string reconst_name = "%reconst_" + std::to_string(split_counter_++);
                reconstructed[use] = reconst_name;

                // Generate reconstruction
                std::string reconst_line = generateReconstruction(split_var, reconst_name);
                result.push_back(reconst_line);
            }

            // Replace original variable names with reconstructed names in the line
            for (const auto& [orig, reconst] : reconstructed) {
                size_t pos = 0;
                while ((pos = working_line.find(orig, pos)) != std::string::npos) {
                    // Make sure we're replacing a complete variable name
                    size_t end_pos = pos + orig.length();
                    bool at_end = end_pos >= working_line.length();
                    bool followed_by_valid = at_end ||
                        (!std::isalnum(working_line[end_pos]) && working_line[end_pos] != '_');

                    // Check it's not the destination variable (at start of line)
                    std::string dest_check = extractDestVariable(working_line);
                    bool is_dest = (dest_check == orig && pos < working_line.find('='));

                    if (followed_by_valid && !is_dest) {
                        working_line.replace(pos, orig.length(), reconst);
                        pos += reconst.length();
                    } else {
                        pos++;
                    }
                }
            }
        }

        // Check if this line defines a split variable
        std::string dest = extractDestVariable(working_line);
        if (!dest.empty() && split_vars_.find(dest) != split_vars_.end()) {
            // Transform the definition (with any uses already reconstructed)
            auto transformed = transformDefinition(working_line, split_vars_[dest]);
            for (const auto& t : transformed) {
                result.push_back(t);
            }
        } else {
            result.push_back(working_line);
        }
    }

    return result;
}

std::vector<std::string> LLVMVariableSplittingPass::transformDefinition(
        const std::string& line,
        const SplitVariable& split_var) {

    std::vector<std::string> result;

    // Check if this is a PHI node - they need special handling
    if (line.find(" phi ") != std::string::npos) {
        return transformPhiNode(line, split_var);
    }

    // Pattern: %dest = op [flags] type operands...
    // Flags like nsw, nuw, exact come before the type
    std::regex def_pattern(R"((\s*)(%[\w.]+)\s*=\s*(\w+)\s+(.*))");
    std::smatch match;

    if (!std::regex_match(line, match, def_pattern)) {
        // Can't parse, return original
        result.push_back(line);
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string op = match[3];
    std::string rest = match[4];

    // Parse flags and type from rest
    // Flags are: nsw, nuw, exact, fast, etc.
    std::string flags;
    std::string type;
    std::string operands;

    // Tokenize rest to extract flags, type, and operands
    std::istringstream iss(rest);
    std::string token;
    bool found_type = false;

    while (iss >> token) {
        // Check if this is a flag
        if (!found_type && (token == "nsw" || token == "nuw" || token == "exact" ||
                           token == "fast" || token == "nnan" || token == "ninf" ||
                           token == "nsz" || token == "arcp" || token == "contract" ||
                           token == "afn" || token == "reassoc")) {
            if (!flags.empty()) flags += " ";
            flags += token;
        } else if (!found_type) {
            // First non-flag token is the type
            type = token;
            found_type = true;
            // Get the rest as operands
            std::string remaining;
            std::getline(iss, remaining);
            // Trim leading whitespace
            size_t start = remaining.find_first_not_of(" \t");
            if (start != std::string::npos) {
                operands = remaining.substr(start);
            }
            break;
        }
    }

    if (type.empty()) {
        // Couldn't parse type, return original
        result.push_back(line);
        return result;
    }

    // First, compute the original value into a temporary
    std::string temp_result = "%split_temp_" + std::to_string(split_counter_++);
    std::string flags_part = flags.empty() ? "" : flags + " ";
    result.push_back(indent + temp_result + " = " + op + " " + flags_part + type + " " + operands);

    // Now split the value
    auto split_lines = generateSplitAssignment(temp_result, type, split_var);
    for (const auto& sl : split_lines) {
        result.push_back(sl);
    }

    logger_.debug("Split definition of {} into {} and {}",
                  dest, split_var.part1_name, split_var.part2_name);

    return result;
}

std::vector<std::string> LLVMVariableSplittingPass::transformUse(
        const std::string& line,
        const std::map<std::string, SplitVariable>& active_splits) {

    std::vector<std::string> result;
    std::string modified_line = line;

    // For each split variable used in this line, we need to reconstruct it first
    std::map<std::string, std::string> reconstructed;

    for (const auto& [orig_name, split_var] : active_splits) {
        if (line.find(orig_name) != std::string::npos) {
            // Need to reconstruct this variable
            std::string reconst_name = "%reconst_" + std::to_string(split_counter_++);
            reconstructed[orig_name] = reconst_name;

            // Generate reconstruction
            std::string reconst_line = generateReconstruction(split_var, reconst_name);
            result.push_back(reconst_line);
        }
    }

    // Replace original variable names with reconstructed names
    for (const auto& [orig, reconst] : reconstructed) {
        size_t pos = 0;
        while ((pos = modified_line.find(orig, pos)) != std::string::npos) {
            // Make sure we're replacing a complete variable name
            size_t end_pos = pos + orig.length();
            bool at_end = end_pos >= modified_line.length();
            bool followed_by_valid = at_end ||
                (!std::isalnum(modified_line[end_pos]) && modified_line[end_pos] != '_');

            if (followed_by_valid) {
                modified_line.replace(pos, orig.length(), reconst);
                pos += reconst.length();
            } else {
                pos++;
            }
        }
    }

    result.push_back(modified_line);
    return result;
}

std::vector<std::string> LLVMVariableSplittingPass::transformPhiNode(
        const std::string& line,
        const SplitVariable& split_var) {

    std::vector<std::string> result;

    // PHI nodes need special handling - we need two PHI nodes for the split parts
    // Format: %result = phi i32 [ %val1, %block1 ], [ %val2, %block2 ], ...

    // Parse the PHI node
    std::regex phi_pattern(R"((\s*)(%[\w.]+)\s*=\s*phi\s+(\w+)\s+(.*))");
    std::smatch match;

    if (!std::regex_match(line, match, phi_pattern)) {
        // Can't parse, return original
        result.push_back(line);
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string type = match[3];
    std::string incoming = match[4];

    // Parse incoming values: [ %val, %block ], [ %val, %block ], ...
    std::regex incoming_pattern(R"(\[\s*([^,\]]+)\s*,\s*([^\]]+)\s*\])");
    std::vector<std::pair<std::string, std::string>> phi_pairs;

    auto begin = std::sregex_iterator(incoming.begin(), incoming.end(), incoming_pattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::string val = (*it)[1].str();
        std::string block = (*it)[2].str();
        // Trim whitespace
        size_t start = val.find_first_not_of(" \t");
        size_t end_pos = val.find_last_not_of(" \t");
        if (start != std::string::npos) {
            val = val.substr(start, end_pos - start + 1);
        }
        start = block.find_first_not_of(" \t");
        end_pos = block.find_last_not_of(" \t");
        if (start != std::string::npos) {
            block = block.substr(start, end_pos - start + 1);
        }
        phi_pairs.push_back({val, block});
    }

    if (phi_pairs.empty()) {
        // Couldn't parse incoming values
        result.push_back(line);
        return result;
    }

    // Generate a random split constant for each incoming value
    // part1 = random values for each predecessor
    // part2 = original - part1 for each predecessor
    // So when reconstructed: part1 + part2 = original

    std::string part1_phi = indent + split_var.part1_name + " = phi " + type + " ";
    std::string part2_phi = indent + split_var.part2_name + " = phi " + type + " ";

    for (size_t i = 0; i < phi_pairs.size(); i++) {
        const auto& [val, block] = phi_pairs[i];

        // For PHI nodes, we use a degenerate split because injecting
        // constants into PHI nodes would require additional computation blocks.
        // part1 = val, part2 = identity element (0 for add/xor, 1 for mul)

        if (split_var.strategy == SplitStrategy::Additive) {
            // part1 = key, part2 = val - key
            // We need the key as a value, so we'll create computation blocks
            // For now, we'll use the simpler approach of not splitting PHI nodes
            // because the key needs to be a constant at compile time

            // Since we can't easily inject constants into PHI nodes without
            // adding extra blocks, we'll use the identity split:
            // part1 = val, part2 = 0
            // This is a degenerate split but maintains correctness

            if (i > 0) {
                part1_phi += ", ";
                part2_phi += ", ";
            }
            part1_phi += "[ " + val + ", " + block + " ]";
            part2_phi += "[ 0, " + block + " ]";
        } else if (split_var.strategy == SplitStrategy::XOR) {
            // XOR split: part1 ^ part2 = val
            // Same issue - we'd need constants per block

            if (i > 0) {
                part1_phi += ", ";
                part2_phi += ", ";
            }
            part1_phi += "[ " + val + ", " + block + " ]";
            part2_phi += "[ 0, " + block + " ]";
        } else {
            // Multiplicative: part1 = val, part2 = 1
            if (i > 0) {
                part1_phi += ", ";
                part2_phi += ", ";
            }
            part1_phi += "[ " + val + ", " + block + " ]";
            part2_phi += "[ 1, " + block + " ]";
        }
    }

    result.push_back(part1_phi);
    result.push_back(part2_phi);

    logger_.debug("Split PHI node {} into {} and {}",
                  dest, split_var.part1_name, split_var.part2_name);

    return result;
}

std::string LLVMVariableSplittingPass::generateReconstruction(
        const SplitVariable& split_var,
        const std::string& result_name) {

    std::string indent = "  ";

    switch (split_var.strategy) {
        case SplitStrategy::Additive:
            // result = part1 + part2
            return indent + result_name + " = add " + split_var.type + " " +
                   split_var.part1_name + ", " + split_var.part2_name;

        case SplitStrategy::XOR:
            // result = part1 ^ part2
            return indent + result_name + " = xor " + split_var.type + " " +
                   split_var.part1_name + ", " + split_var.part2_name;

        case SplitStrategy::Multiplicative:
            // result = part1 * part2
            return indent + result_name + " = mul " + split_var.type + " " +
                   split_var.part1_name + ", " + split_var.part2_name;

        default:
            return indent + result_name + " = add " + split_var.type + " " +
                   split_var.part1_name + ", " + split_var.part2_name;
    }
}

std::vector<std::string> LLVMVariableSplittingPass::generateSplitAssignment(
        const std::string& value,
        const std::string& type,
        const SplitVariable& split_var) {

    std::vector<std::string> result;
    std::string indent = "  ";

    // Generate a random value for part1
    int64_t random_val = GlobalRandom::nextInt(-1000000, 1000000);

    switch (split_var.strategy) {
        case SplitStrategy::Additive:
            // part1 = random constant
            // part2 = value - part1
            result.push_back(indent + split_var.part1_name + " = add " + type +
                             " " + value + ", " + std::to_string(random_val));
            result.push_back(indent + split_var.part2_name + " = sub " + type +
                             " " + value + ", " + split_var.part1_name);
            // Actually this is wrong - we need:
            // part1 = some random transform of value
            // part2 = value - part1
            // Let's fix it:
            break;

        case SplitStrategy::XOR:
            // part1 = value ^ random
            // part2 = random (so part1 ^ part2 = value)
            result.push_back(indent + split_var.part2_name + " = xor " + type +
                             " " + value + ", " + std::to_string(random_val));
            // part2 now equals value ^ random
            // We need part1 = random so that part1 ^ part2 = value
            // Actually: part1 ^ (value ^ random) = value => part1 = random
            // So we store random in part1
            result.push_back(indent + split_var.part1_name + " = add " + type +
                             " 0, " + std::to_string(random_val));
            break;

        case SplitStrategy::Multiplicative: {
            // Multiplicative split using modular arithmetic
            // We use an odd constant factor (so it has a modular multiplicative inverse)
            // value = (value * inv) * factor  (mod 2^32)
            // part1 = value * inv, part2 = factor

            // Pick a random odd number as the factor (must be odd to have inverse mod 2^32)
            int64_t factor = GlobalRandom::nextInt(3, 1000) | 1;  // Ensure odd

            // Extended GCD to find modular inverse of factor mod 2^32
            // For odd numbers, inverse always exists mod 2^32
            // Using the formula: inv = factor * (2 - factor * factor) iteratively
            int64_t inv = factor;
            for (int i = 0; i < 5; i++) {  // 5 iterations is enough for 32-bit
                inv = inv * (2 - factor * inv);
                inv &= 0xFFFFFFFF;  // Keep in 32-bit range
            }

            // part1 = value * inverse (we compute this at runtime)
            // part2 = factor (constant)
            std::string inv_temp = "%split_inv_" + std::to_string(split_counter_++);
            result.push_back(indent + inv_temp + " = add " + type + " 0, " + std::to_string(static_cast<int32_t>(inv)));
            result.push_back(indent + split_var.part1_name + " = mul " + type + " " + value + ", " + inv_temp);
            result.push_back(indent + split_var.part2_name + " = add " + type + " 0, " + std::to_string(factor));
            break;
        }

        default:
            break;
    }

    // Corrected additive split:
    if (split_var.strategy == SplitStrategy::Additive) {
        result.clear();
        // part1 = random constant
        std::string rand_temp = "%split_rand_" + std::to_string(split_counter_++);
        result.push_back(indent + rand_temp + " = add " + type + " 0, " + std::to_string(random_val));
        result.push_back(indent + split_var.part1_name + " = add " + type + " " + rand_temp + ", 0");
        // part2 = value - part1
        result.push_back(indent + split_var.part2_name + " = sub " + type +
                         " " + value + ", " + split_var.part1_name);
    }

    return result;
}

bool LLVMVariableSplittingPass::isExcluded(const std::string& name) const {
    for (const auto& pattern : config_.exclude_patterns) {
        if (name.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string LLVMVariableSplittingPass::extractDestVariable(
        const std::string& line) const {

    std::regex def_pattern(R"(^\s*(%[\w.]+)\s*=)");
    std::smatch match;

    if (std::regex_search(line, match, def_pattern)) {
        return match[1];
    }
    return "";
}

std::string LLVMVariableSplittingPass::extractType(
        const std::string& line) const {

    // Special handling for select instruction:
    // %x = select i1 %cond, i32 %val1, i32 %val2
    // The result type is after the first comma (i32), not the condition type (i1)
    if (line.find(" select ") != std::string::npos) {
        std::regex select_type_pattern(R"(select\s+i1\s+[^,]+,\s*(\w+)\s+)");
        std::smatch match;
        if (std::regex_search(line, match, select_type_pattern)) {
            return match[1];
        }
    }

    // General pattern for most instructions: op type operands
    // e.g., %x = add i32 %a, %b -> captures i32
    std::regex type_pattern(R"(=\s*\w+\s+(\w+)\s+)");
    std::smatch match;

    if (std::regex_search(line, match, type_pattern)) {
        std::string type = match[1];
        // Filter out condition types for instructions that shouldn't be split
        if (type == "i1") {
            return "";  // Don't split boolean values
        }
        return type;
    }
    return "i32";  // Default
}

std::vector<std::string> LLVMVariableSplittingPass::findVariableUses(
        const std::string& line) const {

    std::vector<std::string> uses;
    std::regex var_pattern(R"(%[\w.]+)");

    auto begin = std::sregex_iterator(line.begin(), line.end(), var_pattern);
    auto end = std::sregex_iterator();

    // Skip the first match if it's a definition (destination)
    bool first = true;
    std::string dest = extractDestVariable(line);

    for (auto it = begin; it != end; ++it) {
        std::string var = it->str();
        // Skip destination variable
        if (first && var == dest) {
            first = false;
            continue;
        }
        uses.push_back(var);
        first = false;
    }

    return uses;
}

} // namespace data
} // namespace morphect
