/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * transformation_base.hpp - Base class for all transformation passes
 *
 * All obfuscation passes (MBA, CFF, Bogus CF, etc.) inherit from this
 * base class to ensure consistent interface and behavior.
 */

#ifndef MORPHECT_TRANSFORMATION_BASE_HPP
#define MORPHECT_TRANSFORMATION_BASE_HPP

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <map>
#include <regex>
#include <sstream>

namespace morphect {

// Forward declarations
class Statistics;
class Config;

/**
 * Result of a transformation attempt
 */
enum class TransformResult {
    Success,        // Transformation applied successfully
    Skipped,        // Transformation skipped (probability, config, etc.)
    NotApplicable,  // Pattern didn't match
    Error           // Transformation failed
};

/**
 * Pass priority levels for ordering
 */
enum class PassPriority {
    Early    = 100,   // Run first (e.g., analysis passes)
    Normal   = 500,   // Default priority
    Late     = 900,   // Run last (e.g., cleanup passes)

    // Specific pass priorities
    ControlFlow = 200,  // CFF, Bogus CF run early
    MBA         = 400,  // MBA in middle
    Data        = 600,  // String/constant obfuscation
    Cleanup     = 800   // Dead code, finalization
};

/**
 * Pass type for static dispatch (no RTTI in GCC plugins)
 */
enum class PassType {
    Generic,
    Gimple,
    LLVM,
    Assembly
};

/**
 * Base configuration for all passes
 */
struct PassConfig {
    bool enabled = true;
    double probability = 0.85;  // Global transformation probability
    int verbosity = 1;          // 0=silent, 1=normal, 2=verbose, 3=debug

    // Optional per-function control
    std::vector<std::string> include_functions;  // Only these functions
    std::vector<std::string> exclude_functions;  // Skip these functions
};

/**
 * Abstract base class for all transformation passes
 *
 * Lifecycle:
 *   1. Constructor - basic initialization
 *   2. initialize(config) - load configuration
 *   3. transform(func) - called for each function (multiple times)
 *   4. finalize() - cleanup and final statistics
 */
class TransformationPass {
public:
    virtual ~TransformationPass() = default;

    /**
     * Get the unique name of this pass
     * Used for logging, statistics, and configuration
     */
    virtual std::string getName() const = 0;

    /**
     * Get a description of what this pass does
     */
    virtual std::string getDescription() const = 0;

    /**
     * Get the priority of this pass for ordering
     */
    virtual PassPriority getPriority() const { return PassPriority::Normal; }

    /**
     * Get the type of this pass (for static dispatch without RTTI)
     */
    virtual PassType getPassType() const { return PassType::Generic; }

    /**
     * Get dependencies - passes that must run before this one
     */
    virtual std::vector<std::string> getDependencies() const { return {}; }

    /**
     * Initialize the pass with configuration
     * Called once before any transformations
     *
     * @param config Pass-specific configuration
     * @return true if initialization successful
     */
    virtual bool initialize(const PassConfig& config) {
        config_ = config;
        return true;
    }

    /**
     * Check if this pass is enabled
     */
    bool isEnabled() const { return config_.enabled; }

    /**
     * Set enabled state
     */
    void setEnabled(bool enabled) { config_.enabled = enabled; }

    /**
     * Get current configuration
     */
    const PassConfig& getConfig() const { return config_; }

    /**
     * Finalize the pass after all transformations
     * Called once at the end
     */
    virtual void finalize() {}

    /**
     * Get statistics from this pass
     */
    virtual std::unordered_map<std::string, int> getStatistics() const {
        return statistics_;
    }

    /**
     * Reset statistics (for multi-run scenarios)
     */
    virtual void resetStatistics() {
        statistics_.clear();
    }

protected:
    PassConfig config_;
    std::unordered_map<std::string, int> statistics_;

    /**
     * Increment a statistic counter
     */
    void incrementStat(const std::string& name, int amount = 1) {
        statistics_[name] += amount;
    }

    /**
     * Check if transformation should be applied based on probability
     * Uses the pass's RNG for reproducibility
     */
    bool shouldTransform();

    /**
     * Check if a function should be processed based on include/exclude lists
     */
    bool shouldProcessFunction(const std::string& func_name) const {
        // If include list is specified, function must be in it
        if (!config_.include_functions.empty()) {
            bool found = false;
            for (const auto& f : config_.include_functions) {
                if (f == func_name) { found = true; break; }
            }
            if (!found) return false;
        }

        // Check exclude list
        for (const auto& f : config_.exclude_functions) {
            if (f == func_name) return false;
        }

        return true;
    }
};

/**
 * GIMPLE-specific transformation pass
 * For GCC plugin integration
 */
class GimpleTransformationPass : public TransformationPass {
public:
    PassType getPassType() const override { return PassType::Gimple; }

    /**
     * Transform a GIMPLE function
     *
     * @param func GCC function pointer (cast from void* for portability)
     * @return Result of transformation
     */
    virtual TransformResult transformGimple(void* func) = 0;
};

/**
 * LLVM IR-specific transformation pass
 * For standalone IR obfuscator
 */
class LLVMTransformationPass : public TransformationPass {
public:
    PassType getPassType() const override { return PassType::LLVM; }

    /**
     * Transform LLVM IR lines
     *
     * @param lines Vector of IR lines to transform
     * @return Result of transformation
     */
    virtual TransformResult transformIR(std::vector<std::string>& lines) = 0;

protected:
    /**
     * Renumber sequential SSA values in LLVM IR to fix numbering after transformations
     * LLVM requires numbered SSA values (%0, %1, %2...) to be sequential within each function.
     * Named values (%foo, %bar) don't need renumbering.
     *
     * @param lines Vector of IR lines to renumber (modified in place)
     */
    static void renumberSSA(std::vector<std::string>& lines);

    /**
     * Renumber SSA values within a single function
     *
     * @param func_lines Lines of a single function (excluding define line and closing brace)
     * @param first_arg_num The first argument number (usually 0, or num_args for numbered args)
     * @return Renumbered function lines
     */
    static std::vector<std::string> renumberFunctionSSA(
        const std::vector<std::string>& func_lines,
        int first_arg_num);
};

/**
 * Assembly-specific transformation pass
 * For assembly-level obfuscation
 */
class AssemblyTransformationPass : public TransformationPass {
public:
    PassType getPassType() const override { return PassType::Assembly; }

    /**
     * Transform assembly lines
     *
     * @param lines Vector of assembly lines to transform
     * @param arch Target architecture ("x86_64", "x86_32", "arm64", etc.)
     * @return Result of transformation
     */
    virtual TransformResult transformAssembly(
        std::vector<std::string>& lines,
        const std::string& arch
    ) = 0;
};

// ============================================================================
// SSA Renumbering Implementation (inline for header-only)
// ============================================================================

inline void LLVMTransformationPass::renumberSSA(std::vector<std::string>& lines) {
    std::vector<std::string> result;
    result.reserve(lines.size());

    bool in_function = false;
    std::vector<std::string> func_lines;
    std::string func_define_line;
    int num_args = 0;

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];

        // Detect function start: "define ... @name(args) ... {"
        if (line.find("define ") != std::string::npos && line.find("{") != std::string::npos) {
            in_function = true;
            func_define_line = line;
            func_lines.clear();

            // Count numbered arguments in the define line
            // Arguments look like: i32 noundef %0, i32 noundef %1, ...
            // or: i32 %arg1, i32 %arg2, ...
            std::regex arg_pattern(R"(%(\d+))");
            auto args_begin = std::sregex_iterator(line.begin(), line.end(), arg_pattern);
            auto args_end = std::sregex_iterator();
            num_args = 0;
            for (auto it = args_begin; it != args_end; ++it) {
                int arg_num = std::stoi((*it)[1].str());
                if (arg_num >= num_args) {
                    num_args = arg_num + 1;
                }
            }
            continue;
        }

        // Detect function end
        if (in_function && line.find("}") != std::string::npos &&
            (line.find("define") == std::string::npos) &&
            (line.find("switch") == std::string::npos || line.find("]") != std::string::npos)) {
            // Check if this is really the end of function (not inside a switch)
            // Count braces
            int brace_count = 0;
            for (const auto& fl : func_lines) {
                for (char c : fl) {
                    if (c == '{') brace_count++;
                    else if (c == '}') brace_count--;
                }
            }
            // If braces are balanced, this is end of function
            if (brace_count <= 0) {
                // Renumber the function
                auto renumbered = renumberFunctionSSA(func_lines, num_args);

                // Add the define line and renumbered body
                result.push_back(func_define_line);
                for (const auto& fl : renumbered) {
                    result.push_back(fl);
                }
                result.push_back(line);  // closing brace

                in_function = false;
                func_lines.clear();
                continue;
            }
        }

        if (in_function) {
            func_lines.push_back(line);
        } else {
            result.push_back(line);
        }
    }

    // Handle case where function wasn't closed (shouldn't happen in valid IR)
    if (in_function && !func_lines.empty()) {
        result.push_back(func_define_line);
        for (const auto& fl : func_lines) {
            result.push_back(fl);
        }
    }

    lines = std::move(result);
}

inline std::vector<std::string> LLVMTransformationPass::renumberFunctionSSA(
    const std::vector<std::string>& func_lines,
    int first_arg_num) {

    std::vector<std::string> result;
    result.reserve(func_lines.size());

    // Map from old numbered SSA value to new numbered SSA value
    std::map<int, int> number_map;

    // Reserve entry block number (implicit entry block takes the first number after args)
    int next_number = first_arg_num + 1;

    // First pass: identify all numbered definitions and basic block labels
    std::regex def_pattern(R"(%(\d+)\s*=)");
    std::regex block_label_pattern(R"(^(\d+):)");  // Basic block label at start of line

    for (const auto& line : func_lines) {
        // Check for basic block labels (e.g., "7:" at start of line)
        std::smatch block_match;
        if (std::regex_search(line, block_match, block_label_pattern)) {
            int old_num = std::stoi(block_match[1].str());
            if (number_map.find(old_num) == number_map.end()) {
                number_map[old_num] = next_number++;
            }
        }

        // Check for value definitions
        std::sregex_iterator iter(line.begin(), line.end(), def_pattern);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            int old_num = std::stoi((*iter)[1].str());
            if (number_map.find(old_num) == number_map.end()) {
                number_map[old_num] = next_number++;
            }
        }
    }

    // Second pass: replace all numbered references
    std::regex num_ref(R"(%(\d+))");
    std::regex block_ref(R"(label\s+%(\d+))");  // References to block labels

    for (const auto& line : func_lines) {
        std::string new_line = line;

        // First, handle basic block label at start of line (e.g., "7:" -> "5:")
        std::smatch block_match;
        if (std::regex_search(new_line, block_match, block_label_pattern)) {
            int old_num = std::stoi(block_match[1].str());
            auto it = number_map.find(old_num);
            if (it != number_map.end()) {
                std::string old_label = block_match[0].str();
                std::string new_label = std::to_string(it->second) + ":";
                new_line = std::regex_replace(new_line, block_label_pattern, new_label,
                                               std::regex_constants::format_first_only);
            }
        }

        // Build a list of replacements for %N patterns
        std::vector<std::pair<size_t, std::pair<size_t, std::string>>> replacements;
        std::string::const_iterator search_start = new_line.begin();
        std::smatch match;

        while (std::regex_search(search_start, new_line.cend(), match, num_ref)) {
            int old_num = std::stoi(match[1].str());
            size_t match_pos = match.position() + (search_start - new_line.begin());
            size_t match_len = match[0].length();

            // Check if this number is in our map
            auto it = number_map.find(old_num);
            if (it != number_map.end()) {
                std::string new_ref = "%" + std::to_string(it->second);
                replacements.push_back({match_pos, {match_len, new_ref}});
            }

            search_start = match.suffix().first;
        }

        // Apply replacements in reverse order to maintain positions
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
            size_t pos = it->first;
            size_t len = it->second.first;
            const std::string& replacement = it->second.second;
            new_line.replace(pos, len, replacement);
        }

        result.push_back(new_line);
    }

    return result;
}

} // namespace morphect

#endif // MORPHECT_TRANSFORMATION_BASE_HPP
