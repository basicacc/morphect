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

        // Detect function end
        if (in_function && line.find("}") != std::string::npos &&
            line.find("switch") == std::string::npos) {
            // End of function - process it
            size_t func_end = i;

            // Extract function lines (excluding define and closing brace)
            std::vector<std::string> func_lines;
            for (size_t j = func_start + 1; j < func_end; j++) {
                func_lines.push_back(lines[j]);
            }

            // Transform the function
            auto transformed = transformFunction(lines, func_start, func_end);

            // Add transformed function body (skip define line, already added)
            for (size_t j = 1; j < transformed.size(); j++) {
                result.push_back(transformed[j]);
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
            std::string type = match[3];

            // Skip certain operations that shouldn't be split
            if (op == "alloca" || op == "load" || op == "getelementptr" ||
                op == "call" || op == "invoke" || op == "landingpad" ||
                op == "extractvalue" || op == "insertvalue") {
                continue;
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

    // Analyze variables
    auto analyses = analyzeVariables(lines, func_start, func_end);

    // Select variables to split
    auto to_split = selectVariablesToSplit(analyses);

    if (to_split.empty()) {
        // No variables to split, return original
        for (size_t i = func_start; i <= func_end && i < lines.size(); i++) {
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

    // Transform lines
    for (size_t i = func_start; i <= func_end && i < lines.size(); i++) {
        const std::string& line = lines[i];

        // Check if this line defines a split variable
        std::string dest = extractDestVariable(line);
        if (!dest.empty() && split_vars_.find(dest) != split_vars_.end()) {
            // Transform the definition
            auto transformed = transformDefinition(line, split_vars_[dest]);
            for (const auto& t : transformed) {
                result.push_back(t);
            }
            continue;
        }

        // Check if this line uses any split variables
        auto uses = findVariableUses(line);
        bool has_split_use = false;
        for (const auto& use : uses) {
            if (split_vars_.find(use) != split_vars_.end()) {
                has_split_use = true;
                break;
            }
        }

        if (has_split_use) {
            // Transform uses
            auto transformed = transformUse(line, split_vars_);
            for (const auto& t : transformed) {
                result.push_back(t);
            }
        } else {
            result.push_back(line);
        }
    }

    return result;
}

std::vector<std::string> LLVMVariableSplittingPass::transformDefinition(
        const std::string& line,
        const SplitVariable& split_var) {

    std::vector<std::string> result;

    // Pattern: %dest = op type operands...
    std::regex def_pattern(R"((\s*)(%[\w.]+)\s*=\s*(\w+)\s+(\w+)\s+(.*))");
    std::smatch match;

    if (!std::regex_match(line, match, def_pattern)) {
        // Can't parse, return original
        result.push_back(line);
        return result;
    }

    std::string indent = match[1];
    std::string dest = match[2];
    std::string op = match[3];
    std::string type = match[4];
    std::string operands = match[5];

    // First, compute the original value into a temporary
    std::string temp_result = "%split_temp_" + std::to_string(split_counter_++);
    result.push_back(indent + temp_result + " = " + op + " " + type + " " + operands);

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
    // This is complex because we need to track which predecessor contributed which value

    // For simplicity, we'll skip PHI node splitting in this implementation
    // and just mark them as non-splittable

    result.push_back(line);
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

        case SplitStrategy::Multiplicative:
            // For multiplicative, we need factors - complex, skip for now
            result.push_back(indent + split_var.part1_name + " = add " + type + " " + value + ", 0");
            result.push_back(indent + split_var.part2_name + " = add " + type + " 1, 0");
            break;

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

    std::regex type_pattern(R"(=\s*\w+\s+(\w+)\s+)");
    std::smatch match;

    if (std::regex_search(line, match, type_pattern)) {
        return match[1];
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
