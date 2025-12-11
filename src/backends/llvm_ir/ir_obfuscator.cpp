/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * ir_obfuscator.cpp - LLVM IR Obfuscator (Standalone Tool)
 *
 * This is a standalone tool that reads LLVM IR (.ll files),
 * applies obfuscation transformations, and outputs obfuscated IR.
 *
 * Supported transformations:
 *   - MBA (Mixed Boolean Arithmetic) - pattern-based
 *   - CFF (Control Flow Flattening)
 *   - Bogus Control Flow
 *   - Variable Splitting
 *   - String Encoding
 *   - Dead Code Insertion
 *
 * Usage:
 *   morphect-ir --config config.json input.ll output.ll
 *
 * Workflow:
 *   1. clang -S -emit-llvm -O0 source.c -o source.ll
 *   2. morphect-ir --config config.json source.ll obfuscated.ll
 *   3. llc obfuscated.ll -o obfuscated.s
 *   4. gcc obfuscated.s -o output
 */

#include "morphect.hpp"
#include "passes/cff/cff.hpp"
#include "passes/data/data.hpp"
#include "passes/deadcode/deadcode.hpp"
#include "passes/mba/mba_pass.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>
#include <map>
#include <algorithm>

using namespace morphect;

/**
 * LLVM IR Transformation Rule
 */
struct IRRule {
    std::string pattern;
    std::vector<std::vector<std::string>> replacements;
    std::vector<double> probabilities;
    std::string description;
};

/**
 * LLVM IR Obfuscator class
 */
class LLVMIRObfuscator {
public:
    LLVMIRObfuscator() : logger_("IRObfuscator") {}

    bool loadConfig(const std::string& config_file) {
        try {
            auto json = JsonParser::parseFile(config_file);
            parseConfig(json);
            return true;
        } catch (const std::exception& e) {
            logger_.error("Failed to load config: {}", e.what());
            return false;
        }
    }

    void setGlobalProbability(double prob) {
        global_probability_ = prob;
    }

    std::string obfuscate(const std::string& ir_code) {
        std::istringstream input(ir_code);
        std::vector<std::string> result;
        std::string line;
        int line_count = 0;

        while (std::getline(input, line)) {
            line_count++;
            std::string trimmed = trim(line);

            // Skip metadata, declarations, etc.
            if (shouldSkipLine(trimmed)) {
                result.push_back(line);
                continue;
            }

            // Try to apply transformations
            bool transformed = false;

            for (const auto& [name, rule] : rules_) {
                std::map<std::string, std::string> vars;

                if (matchPattern(trimmed, rule.pattern, vars)) {
                    // Check probability
                    if (!GlobalRandom::decide(global_probability_)) {
                        continue;
                    }

                    auto replacement = applyRule(rule, vars, line);
                    if (!replacement.empty()) {
                        std::string indent = getIndent(line);

                        for (const auto& repl_line : replacement) {
                            result.push_back(indent + repl_line);
                        }

                        transformed = true;
                        stats_.increment(name + "_applied");
                        logger_.debug("Applied {} at line {}", name, line_count);
                        break;
                    }
                }
            }

            if (!transformed) {
                result.push_back(line);
            }
        }

        // Build output
        std::ostringstream output;
        for (size_t i = 0; i < result.size(); i++) {
            output << result[i];
            if (i < result.size() - 1) output << "\n";
        }
        output << "\n";

        return output.str();
    }

    void printStatistics() {
        logger_.info("");
        logger_.info("=== LLVM IR Obfuscation Statistics ===");

        int total = 0;
        for (const auto& [name, count] : stats_.getIntStats()) {
            logger_.info("  {}: {}", name, count);
            total += count;
        }

        logger_.info("Total transformations: {}", total);
        logger_.info("======================================");
    }

    // Enable/disable individual passes
    void setEnableMBA(bool enable) { enable_mba_ = enable; }
    void setEnableCFF(bool enable) { enable_cff_ = enable; }
    void setEnableBogus(bool enable) { enable_bogus_ = enable; }
    void setEnableVariableSplit(bool enable) { enable_var_split_ = enable; }
    void setEnableStringEncoding(bool enable) { enable_string_enc_ = enable; }
    void setEnableDeadCode(bool enable) { enable_dead_code_ = enable; }

    /**
     * Full obfuscation pipeline with all enabled passes
     */
    std::string obfuscateFull(const std::string& ir_code) {
        std::string result = ir_code;

        // Convert to line vector for pass-based transformations
        std::vector<std::string> lines = splitLines(result);

        // Step 1: Apply MBA transformations (using LLVMMBAPass)
        if (enable_mba_) {
            lines = applyMBA(lines);
        }

        // Step 2: Apply CFF (Control Flow Flattening)
        if (enable_cff_) {
            lines = applyCFF(lines);
        }

        // Step 3: Apply Bogus Control Flow
        if (enable_bogus_) {
            lines = applyBogusControlFlow(lines);
        }

        // Step 4: Apply Variable Splitting
        if (enable_var_split_) {
            lines = applyVariableSplitting(lines);
        }

        // Step 5: Apply String Encoding
        if (enable_string_enc_) {
            lines = applyStringEncoding(lines);
        }

        // Step 6: Apply Dead Code Insertion
        if (enable_dead_code_) {
            lines = applyDeadCode(lines);
        }

        return joinLines(lines);
    }

private:
    // Pass enable flags
    bool enable_mba_ = false;
    bool enable_cff_ = false;
    bool enable_bogus_ = false;
    bool enable_var_split_ = false;
    bool enable_string_enc_ = false;
    bool enable_dead_code_ = false;

    /**
     * Apply MBA (Mixed Boolean Arithmetic) pass
     */
    std::vector<std::string> applyMBA(const std::vector<std::string>& lines) {
        logger_.info("Applying MBA transformations...");

        mba::LLVMMBAPass mba_pass;
        mba::MBAPassConfig mba_config;
        mba_config.global.enabled = true;
        mba_config.global.probability = global_probability_;
        mba_config.global.nesting_depth = 1;  // Single pass - nesting causes correctness issues
        mba_pass.initializeMBA(mba_config);

        std::vector<std::string> result = lines;
        auto transform_result = mba_pass.transformIR(result);

        if (transform_result == TransformResult::Success) {
            auto mba_stats = mba_pass.getStatistics();
            for (const auto& [key, val] : mba_stats) {
                stats_.increment("mba_" + key, val);
            }
            logger_.info("MBA applied successfully");
        }

        return result;
    }

    /**
     * Apply Control Flow Flattening pass
     */
    std::vector<std::string> applyCFF(const std::vector<std::string>& lines) {
        logger_.info("Applying Control Flow Flattening...");

        cff::LLVMCFFPass cff_pass;
        cff::CFFConfig cff_config;
        cff_config.enabled = true;
        cff_config.probability = global_probability_;
        cff_config.shuffle_states = true;
        cff_config.add_bogus_cases = true;
        cff_pass.setCFFConfig(cff_config);

        std::vector<std::string> result = lines;
        auto transform_result = cff_pass.transformIR(result);

        if (transform_result == TransformResult::Success) {
            auto cff_stats = cff_pass.getStatistics();
            for (const auto& [key, val] : cff_stats) {
                stats_.increment("cff_" + key, val);
            }
            logger_.info("CFF applied successfully");
        }

        return result;
    }

    /**
     * Apply Bogus Control Flow pass
     */
    std::vector<std::string> applyBogusControlFlow(const std::vector<std::string>& lines) {
        logger_.info("Applying Bogus Control Flow...");

        cff::LLVMBogusPass bogus_pass;
        cff::BogusConfig bogus_config;
        bogus_config.enabled = true;
        bogus_config.probability = global_probability_;
        bogus_config.generate_dead_code = true;
        bogus_config.use_real_variables = true;
        bogus_pass.setBogusConfig(bogus_config);

        std::vector<std::string> result = lines;
        auto transform_result = bogus_pass.transformIR(result);

        if (transform_result == TransformResult::Success) {
            auto bogus_stats = bogus_pass.getStatistics();
            for (const auto& [key, val] : bogus_stats) {
                stats_.increment("bogus_" + key, val);
            }
            logger_.info("Bogus CF applied successfully");
        }

        return result;
    }

    /**
     * Apply Variable Splitting pass
     */
    std::vector<std::string> applyVariableSplitting(const std::vector<std::string>& lines) {
        logger_.info("Applying Variable Splitting...");

        data::LLVMVariableSplittingPass split_pass;
        data::VariableSplittingConfig split_config;
        split_config.enabled = true;
        split_config.probability = global_probability_;
        split_config.split_phi_nodes = true;
        split_config.max_splits_per_function = 5;
        // Exclude internal obfuscation variables from splitting
        split_config.exclude_patterns = {
            "%_op_",      // Opaque predicate variables
            "%_cff_",     // CFF state variables
            "%_dead",     // Dead code variables
            "%split_",    // Already split variables
            "%reconst_",  // Reconstruction variables
            "%_arith"     // Dead code arithmetic
        };
        split_pass.configure(split_config);

        std::vector<std::string> result = lines;
        auto transform_result = split_pass.transformIR(result);

        if (transform_result == TransformResult::Success) {
            auto split_stats = split_pass.getStatistics();
            for (const auto& [key, val] : split_stats) {
                stats_.increment("varsplit_" + key, val);
            }
            logger_.info("Variable splitting applied successfully");
        }

        return result;
    }

    /**
     * Apply String Encoding pass
     */
    std::vector<std::string> applyStringEncoding(const std::vector<std::string>& lines) {
        logger_.info("Applying String Encoding...");

        data::LLVMStringEncodingPass str_pass;
        data::StringEncodingConfig str_config;
        str_config.enabled = true;
        str_config.method = data::StringEncodingMethod::XOR;
        str_config.min_string_length = 3;
        str_pass.configure(str_config);

        std::vector<std::string> result = lines;
        auto transform_result = str_pass.transformIR(result);

        if (transform_result == TransformResult::Success) {
            auto str_stats = str_pass.getStatistics();
            for (const auto& [key, val] : str_stats) {
                stats_.increment("strenc_" + key, val);
            }
            logger_.info("String encoding applied successfully");
        }

        return result;
    }

    /**
     * Apply Dead Code Insertion pass
     */
    std::vector<std::string> applyDeadCode(const std::vector<std::string>& lines) {
        logger_.info("Applying Dead Code Insertion...");

        deadcode::LLVMDeadCodePass dead_pass;
        deadcode::DeadCodeConfig dead_config;
        dead_config.enabled = true;
        dead_config.probability = global_probability_;
        dead_config.max_blocks = 5;
        dead_pass.setDeadCodeConfig(dead_config);

        std::vector<std::string> result = lines;
        auto transform_result = dead_pass.transformIR(result);

        if (transform_result == TransformResult::Success) {
            auto dead_stats = dead_pass.getStatistics();
            for (const auto& [key, val] : dead_stats) {
                stats_.increment("deadcode_" + key, val);
            }
            logger_.info("Dead code insertion applied successfully");
        }

        return result;
    }

    std::vector<std::string> splitLines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    std::string joinLines(const std::vector<std::string>& lines) {
        std::ostringstream result;
        for (size_t i = 0; i < lines.size(); i++) {
            result << lines[i];
            if (i < lines.size() - 1) result << "\n";
        }
        result << "\n";
        return result.str();
    }

private:
    Logger logger_;
    Statistics stats_;
    std::map<std::string, IRRule> rules_;
    double global_probability_ = 0.85;
    int temp_counter_ = 10000;

    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }

    std::string getIndent(const std::string& line) {
        size_t pos = line.find_first_not_of(" \t");
        return pos != std::string::npos ? line.substr(0, pos) : "";
    }

    bool shouldSkipLine(const std::string& trimmed) {
        if (trimmed.empty()) return true;
        if (trimmed[0] == ';') return true;  // Comment
        if (trimmed[0] == '!') return true;  // Metadata
        if (trimmed.find("define") == 0) return true;
        if (trimmed.find("declare") == 0) return true;
        if (trimmed.find("attributes") == 0) return true;
        if (trimmed.find("target") == 0) return true;
        if (trimmed.find("source_filename") == 0) return true;
        if (trimmed.find("ret ") == 0) return true;
        if (trimmed.find("store ") == 0) return true;
        if (trimmed.find("load ") == 0) return true;
        if (trimmed.find("alloca ") == 0) return true;
        if (trimmed.find("call ") == 0) return true;
        if (trimmed.find("br ") == 0) return true;
        if (trimmed.find("}") == 0) return true;
        if (trimmed.find(":") != std::string::npos &&
            trimmed.find("=") == std::string::npos) return true;  // Label

        return false;
    }

    void parseConfig(const JsonValue& json) {
        // Get global probability
        if (json.has("global_probability")) {
            global_probability_ = json["global_probability"].asDouble(0.85);
        }

        const JsonValue& settings = json.has("obfuscation_settings")
            ? json["obfuscation_settings"]
            : json;

        if (settings.has("global_probability")) {
            global_probability_ = settings["global_probability"].asDouble(0.85);
        }

        // Parse pass enable flags from control_flow section
        if (json.has("control_flow")) {
            const auto& cf = json["control_flow"];
            if (cf.has("cff_enabled")) {
                enable_cff_ = cf["cff_enabled"].asBool(false);
            }
            if (cf.has("bogus_cf_enabled")) {
                enable_bogus_ = cf["bogus_cf_enabled"].asBool(false);
            }
        }

        // Parse data obfuscation settings
        if (json.has("data_obfuscation")) {
            const auto& data = json["data_obfuscation"];
            if (data.has("variable_splitting")) {
                enable_var_split_ = data["variable_splitting"].asBool(false);
            }
            if (data.has("string_encoding")) {
                enable_string_enc_ = data["string_encoding"].asBool(false);
            }
        }

        // Parse dead code settings
        if (json.has("dead_code")) {
            const auto& dc = json["dead_code"];
            if (dc.has("enabled")) {
                enable_dead_code_ = dc["enabled"].asBool(false);
            }
        }

        // Parse transformation rules
        if (json.has("ir_transformations")) {
            parseTransformations(json["ir_transformations"]);
        }

        logger_.info("Loaded {} transformation rules", rules_.size());
        logger_.info("CFF: {}, Bogus: {}, VarSplit: {}, StrEnc: {}, DeadCode: {}",
                     enable_cff_ ? "on" : "off",
                     enable_bogus_ ? "on" : "off",
                     enable_var_split_ ? "on" : "off",
                     enable_string_enc_ ? "on" : "off",
                     enable_dead_code_ ? "on" : "off");
    }

    void parseTransformations(const JsonValue& transforms) {
        // Parse each category
        std::vector<std::string> categories = {
            "mba_transformations",
            "comparison_ops",
            "constant_ops",
            "identity_ops"
        };

        for (const auto& cat : categories) {
            if (transforms.has(cat)) {
                parseCategory(transforms[cat], cat);
            }
        }
    }

    void parseCategory(const JsonValue& category, const std::string& cat_name) {
        if (!category.isObject()) return;

        for (const auto& [pattern, rule_json] : category.object_value) {
            IRRule rule;
            rule.pattern = pattern;

            if (rule_json.has("description")) {
                rule.description = rule_json["description"].asString();
            }

            if (rule_json.has("replacements") && rule_json["replacements"].isArray()) {
                for (const auto& repl : rule_json["replacements"].array_value) {
                    rule.replacements.push_back(repl.asStringArray());
                }
            }

            if (rule_json.has("probabilities")) {
                rule.probabilities = rule_json["probabilities"].asDoubleArray();
            }

            // Ensure probabilities match replacements
            if (rule.probabilities.size() != rule.replacements.size()) {
                double equal_prob = 1.0 / rule.replacements.size();
                rule.probabilities.assign(rule.replacements.size(), equal_prob);
            }

            std::string rule_name = cat_name + "_" + std::to_string(rules_.size());
            rules_[rule_name] = rule;
        }
    }

    bool matchPattern(const std::string& line, const std::string& pattern,
                     std::map<std::string, std::string>& vars) {
        // Convert pattern to regex
        std::string regex_str = patternToRegex(pattern);

        try {
            std::regex re(regex_str);
            std::smatch match;

            if (std::regex_search(line, match, re)) {
                // Extract variables
                extractVariables(pattern, match, vars);
                return true;
            }
        } catch (const std::regex_error& e) {
            logger_.error("Regex error: {}", e.what());
        }

        return false;
    }

    std::string patternToRegex(const std::string& pattern) {
        std::string result = pattern;

        // Escape special regex characters
        std::vector<std::pair<std::string, std::string>> escapes = {
            {"\\", "\\\\"}, {"^", "\\^"}, {"$", "\\$"},
            {".", "\\."}, {"|", "\\|"}, {"?", "\\?"},
            {"*", "\\*"}, {"+", "\\+"}, {"(", "\\("},
            {")", "\\)"}, {"[", "\\["}, {"]", "\\]"}
        };

        for (const auto& [ch, escaped] : escapes) {
            size_t pos = 0;
            while ((pos = result.find(ch, pos)) != std::string::npos) {
                result.replace(pos, ch.length(), escaped);
                pos += escaped.length();
            }
        }

        // Replace {var} with capture groups
        std::regex var_re("\\\\\\{([^}]+)\\\\\\}");
        result = std::regex_replace(result, var_re, "([a-zA-Z0-9_%.]+)");

        // Replace spaces with flexible whitespace
        size_t pos = 0;
        while ((pos = result.find(" ", pos)) != std::string::npos) {
            result.replace(pos, 1, "\\s+");
            pos += 3;
        }

        return result;
    }

    void extractVariables(const std::string& pattern, const std::smatch& match,
                         std::map<std::string, std::string>& vars) {
        std::regex var_re("\\{([^}]+)\\}");
        std::sregex_iterator iter(pattern.begin(), pattern.end(), var_re);
        std::sregex_iterator end;

        size_t i = 1;
        for (; iter != end && i < match.size(); ++iter, ++i) {
            vars[iter->str(1)] = match[i].str();
        }
    }

    std::vector<std::string> applyRule(const IRRule& rule,
                                        const std::map<std::string, std::string>& vars,
                                        const std::string& /*original*/) {
        if (rule.replacements.empty()) return {};

        // Select replacement based on probability
        size_t idx = GlobalRandom::get().chooseWeighted(
            rule.replacements, rule.probabilities);

        const auto& replacement = rule.replacements[idx];

        // Substitute variables
        std::map<std::string, std::string> temp_vars;
        std::vector<std::string> result;

        for (const auto& line : replacement) {
            std::string substituted = line;

            // Replace known variables
            for (const auto& [name, value] : vars) {
                std::string placeholder = "{" + name + "}";
                size_t pos = 0;
                while ((pos = substituted.find(placeholder, pos)) != std::string::npos) {
                    substituted.replace(pos, placeholder.length(), value);
                    pos += value.length();
                }
            }

            // Replace temp variables
            std::regex temp_re("\\{(temp[0-9]+)\\}");
            std::smatch temp_match;
            std::string scan = substituted;

            while (std::regex_search(scan, temp_match, temp_re)) {
                std::string temp_name = temp_match[1].str();
                if (temp_vars.find(temp_name) == temp_vars.end()) {
                    temp_vars[temp_name] = std::to_string(temp_counter_++);
                }
                scan = temp_match.suffix().str();
            }

            for (const auto& [name, value] : temp_vars) {
                std::string placeholder = "{" + name + "}";
                size_t pos = 0;
                while ((pos = substituted.find(placeholder, pos)) != std::string::npos) {
                    substituted.replace(pos, placeholder.length(), value);
                    pos += value.length();
                }
            }

            result.push_back(substituted);
        }

        return result;
    }
};

/**
 * Print usage
 */
void printUsage(const char* program) {
    std::cout << getBanner() << std::endl;
    std::cout << "Usage: " << program << " [options] <input.ll> <output.ll>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --config <file>       Configuration file (JSON)" << std::endl;
    std::cout << "  --probability <n>     Global transformation probability (0.0-1.0)" << std::endl;
    std::cout << "  --mba                 Enable MBA (Mixed Boolean Arithmetic) transformations" << std::endl;
    std::cout << "  --cff                 Enable Control Flow Flattening" << std::endl;
    std::cout << "  --bogus               Enable Bogus Control Flow" << std::endl;
    std::cout << "  --varsplit            Enable Variable Splitting" << std::endl;
    std::cout << "  --strenc              Enable String Encoding" << std::endl;
    std::cout << "  --deadcode            Enable Dead Code Insertion" << std::endl;
    std::cout << "  --all                 Enable all obfuscation passes" << std::endl;
    std::cout << "  --verbose             Enable verbose output" << std::endl;
    std::cout << "  --help                Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Workflow:" << std::endl;
    std::cout << "  1. clang -S -emit-llvm -O0 source.c -o source.ll" << std::endl;
    std::cout << "  2. " << program << " --all source.ll obfuscated.ll" << std::endl;
    std::cout << "  3. llc obfuscated.ll -o obfuscated.s" << std::endl;
    std::cout << "  4. gcc obfuscated.s -o output" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file;
    std::string input_file;
    std::string output_file;
    double probability = -1;
    bool verbose = false;
    bool enable_mba = false;
    bool enable_cff = false;
    bool enable_bogus = false;
    bool enable_varsplit = false;
    bool enable_strenc = false;
    bool enable_deadcode = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--probability" && i + 1 < argc) {
            probability = std::stod(argv[++i]);
        } else if (arg == "--mba") {
            enable_mba = true;
        } else if (arg == "--cff") {
            enable_cff = true;
        } else if (arg == "--bogus") {
            enable_bogus = true;
        } else if (arg == "--varsplit") {
            enable_varsplit = true;
        } else if (arg == "--strenc") {
            enable_strenc = true;
        } else if (arg == "--deadcode") {
            enable_deadcode = true;
        } else if (arg == "--all") {
            enable_mba = true;
            enable_cff = true;
            enable_bogus = true;
            enable_varsplit = true;
            enable_strenc = true;
            enable_deadcode = true;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg[0] != '-') {
            if (input_file.empty()) {
                input_file = arg;
            } else if (output_file.empty()) {
                output_file = arg;
            }
        }
    }

    if (input_file.empty() || output_file.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    // Set up logging
    if (verbose) {
        LogConfig::get().setLevel(LogLevel::Debug);
    }

    printBanner();

    // Create obfuscator
    LLVMIRObfuscator obfuscator;

    // Load config
    if (!config_file.empty()) {
        if (!obfuscator.loadConfig(config_file)) {
            return 1;
        }
    }

    if (probability >= 0) {
        obfuscator.setGlobalProbability(probability);
    }

    // Apply command-line pass enables (override config)
    if (enable_mba) obfuscator.setEnableMBA(true);
    if (enable_cff) obfuscator.setEnableCFF(true);
    if (enable_bogus) obfuscator.setEnableBogus(true);
    if (enable_varsplit) obfuscator.setEnableVariableSplit(true);
    if (enable_strenc) obfuscator.setEnableStringEncoding(true);
    if (enable_deadcode) obfuscator.setEnableDeadCode(true);

    // Read input
    std::ifstream input(input_file);
    if (!input.is_open()) {
        LOG_ERROR("Cannot open input file: {}", input_file);
        return 1;
    }

    std::string ir_code((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    input.close();

    LOG_INFO("Read {} bytes from {}", ir_code.size(), input_file);

    // Obfuscate using full pipeline (MBA + CFF + Bogus + VarSplit + StrEnc + DeadCode)
    std::string obfuscated = obfuscator.obfuscateFull(ir_code);

    // Write output
    std::ofstream output(output_file);
    if (!output.is_open()) {
        LOG_ERROR("Cannot create output file: {}", output_file);
        return 1;
    }

    output << obfuscated;
    output.close();

    LOG_INFO("Wrote {} bytes to {}", obfuscated.size(), output_file);

    // Statistics
    double increase = ((double)obfuscated.size() / ir_code.size() - 1.0) * 100.0;
    LOG_INFO("Code size increase: {:.1f}%", increase);

    obfuscator.printStatistics();

    return 0;
}
