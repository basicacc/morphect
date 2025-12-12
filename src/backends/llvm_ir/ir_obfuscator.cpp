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
#include <set>
#include <algorithm>
#include <iomanip>

using namespace morphect;

// Global verbose flag for detailed output
static bool g_verbose = false;

/**
 * Transformation record for detailed logging
 */
struct TransformationRecord {
    std::string function_name;
    std::string pass_name;
    std::string operation;
    std::string original;
    std::string transformed;
    int line_number;
};

/**
 * Function info extracted from IR
 */
struct FunctionInfo {
    std::string name;
    int start_line;
    int end_line;
    int instruction_count;
    std::vector<std::string> basic_blocks;
};

/**
 * LLVM IR Obfuscator class with detailed logging
 */
class LLVMIRObfuscator {
public:
    LLVMIRObfuscator() : logger_("morphect") {}

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

    void setVerbose(bool v) { verbose_ = v; g_verbose = v; }
    void setGlobalProbability(double prob) { global_probability_ = prob; }

    // Enable/disable individual passes
    void setEnableMBA(bool enable) { enable_mba_ = enable; }
    void setEnableCFF(bool enable) { enable_cff_ = enable; }
    void setEnableBogus(bool enable) { enable_bogus_ = enable; }
    void setEnableVariableSplit(bool enable) { enable_var_split_ = enable; }
    void setEnableStringEncoding(bool enable) { enable_string_enc_ = enable; }
    void setEnableDeadCode(bool enable) { enable_dead_code_ = enable; }

    /**
     * Full obfuscation pipeline with detailed logging
     */
    std::string obfuscateFull(const std::string& ir_code) {
        transformations_.clear();
        pass_stats_.clear();

        // Parse functions from IR
        std::vector<std::string> lines = splitLines(ir_code);
        std::vector<FunctionInfo> functions = parseFunctions(lines);

        if (verbose_) {
            logHeader("IR Analysis");
            fprintf(stderr, "[morphect] Found %zu functions in input\n", functions.size());
            for (const auto& func : functions) {
                fprintf(stderr, "[morphect]   - %s (%d instructions, %zu basic blocks)\n",
                        func.name.c_str(), func.instruction_count, func.basic_blocks.size());
            }
            fprintf(stderr, "\n");
        }

        // Apply passes with detailed logging
        if (enable_mba_) {
            lines = applyMBAPass(lines, functions);
        }

        if (enable_cff_) {
            lines = applyCFFPass(lines, functions);
        }

        if (enable_bogus_) {
            lines = applyBogusPass(lines, functions);
        }

        if (enable_var_split_) {
            lines = applyVariableSplitPass(lines, functions);
        }

        if (enable_string_enc_) {
            lines = applyStringEncodingPass(lines, functions);
        }

        if (enable_dead_code_) {
            lines = applyDeadCodePass(lines, functions);
        }

        return joinLines(lines);
    }

    /**
     * Print detailed statistics like GIMPLE plugin
     */
    void printStatistics() {
        fprintf(stderr, "\n");
        logHeader("Transformation Summary");

        int total = 0;

        // Print per-pass statistics
        for (const auto& [pass, count] : pass_stats_) {
            fprintf(stderr, "[morphect] %s: %d transformations\n", pass.c_str(), count);
            total += count;
        }

        fprintf(stderr, "[morphect] ─────────────────────────────────\n");
        fprintf(stderr, "[morphect] Total transformations: %d\n", total);

        // Print per-function breakdown if verbose
        if (verbose_ && !transformations_.empty()) {
            fprintf(stderr, "\n");
            logHeader("Per-Function Details");

            std::map<std::string, std::vector<TransformationRecord>> by_function;
            for (const auto& t : transformations_) {
                by_function[t.function_name].push_back(t);
            }

            for (const auto& [func, records] : by_function) {
                fprintf(stderr, "[morphect] Function: %s\n", func.c_str());
                std::map<std::string, int> pass_counts;
                for (const auto& r : records) {
                    pass_counts[r.pass_name]++;
                }
                for (const auto& [pass, count] : pass_counts) {
                    fprintf(stderr, "[morphect]   %s: %d\n", pass.c_str(), count);
                }
            }
        }

        fprintf(stderr, "\n");
    }

private:
    Logger logger_;
    bool verbose_ = false;
    double global_probability_ = 0.85;

    // Pass enable flags
    bool enable_mba_ = false;
    bool enable_cff_ = false;
    bool enable_bogus_ = false;
    bool enable_var_split_ = false;
    bool enable_string_enc_ = false;
    bool enable_dead_code_ = false;

    // Tracking
    std::vector<TransformationRecord> transformations_;
    std::map<std::string, int> pass_stats_;

    void logHeader(const std::string& title) {
        fprintf(stderr, "[morphect] ═══════════════════════════════════\n");
        fprintf(stderr, "[morphect] %s\n", title.c_str());
        fprintf(stderr, "[morphect] ═══════════════════════════════════\n");
    }

    void logPassStart(const std::string& pass_name) {
        fprintf(stderr, "[morphect] ┌─ %s\n", pass_name.c_str());
    }

    void logPassEnd(const std::string& pass_name, int count) {
        fprintf(stderr, "[morphect] └─ %s complete: %d transformations\n\n", pass_name.c_str(), count);
    }

    void logTransformation(const std::string& func, const std::string& pass,
                          const std::string& op, const std::string& detail) {
        if (verbose_) {
            fprintf(stderr, "[morphect] │  [%s] %s: %s\n", func.c_str(), op.c_str(), detail.c_str());
        }
        pass_stats_[pass]++;
    }

    /**
     * Parse functions from LLVM IR
     */
    std::vector<FunctionInfo> parseFunctions(const std::vector<std::string>& lines) {
        std::vector<FunctionInfo> functions;
        std::regex define_re(R"(define\s+.*@([\w.]+)\s*\()");
        std::regex bb_re(R"(^(\w+):)");

        FunctionInfo* current = nullptr;
        int instr_count = 0;

        for (size_t i = 0; i < lines.size(); i++) {
            const std::string& line = lines[i];
            std::string trimmed = trim(line);

            std::smatch match;
            if (std::regex_search(line, match, define_re)) {
                if (current) {
                    current->end_line = static_cast<int>(i) - 1;
                    current->instruction_count = instr_count;
                }
                functions.push_back(FunctionInfo{});
                current = &functions.back();
                current->name = match[1].str();
                current->start_line = static_cast<int>(i);
                instr_count = 0;
            } else if (current) {
                if (trimmed == "}") {
                    current->end_line = static_cast<int>(i);
                    current->instruction_count = instr_count;
                    current = nullptr;
                } else if (std::regex_match(trimmed, match, bb_re)) {
                    current->basic_blocks.push_back(match[1].str());
                } else if (!trimmed.empty() && trimmed[0] != ';' && trimmed[0] != '!') {
                    // Count as instruction
                    if (trimmed.find('=') != std::string::npos ||
                        trimmed.find("ret ") == 0 ||
                        trimmed.find("br ") == 0 ||
                        trimmed.find("store ") == 0 ||
                        trimmed.find("call ") == 0) {
                        instr_count++;
                    }
                }
            }
        }

        return functions;
    }

    /**
     * Get function name at a given line
     */
    std::string getFunctionAt(const std::vector<FunctionInfo>& functions, int line) {
        for (const auto& func : functions) {
            if (line >= func.start_line && line <= func.end_line) {
                return func.name;
            }
        }
        return "<global>";
    }

    /**
     * Apply MBA Pass with detailed logging
     */
    std::vector<std::string> applyMBAPass(const std::vector<std::string>& lines,
                                           const std::vector<FunctionInfo>& functions) {
        logPassStart("MBA (Mixed Boolean Arithmetic)");

        // Log functions being processed
        if (verbose_) {
            for (const auto& func : functions) {
                fprintf(stderr, "[morphect] │  Processing: %s\n", func.name.c_str());
            }
        }

        mba::LLVMMBAPass mba_pass;
        mba::MBAPassConfig mba_config;
        mba_config.global.enabled = true;
        mba_config.global.probability = global_probability_;
        mba_config.global.nesting_depth = 1;
        mba_pass.initializeMBA(mba_config);

        std::vector<std::string> result = lines;
        int transformations = 0;

        // Apply MBA to all lines at once
        auto transform_result = mba_pass.transformIR(result);

        if (transform_result == TransformResult::Success) {
            auto stats = mba_pass.getStatistics();

            // Log each type of transformation
            for (const auto& [key, val] : stats) {
                if (val > 0 && key.find("_applied") != std::string::npos) {
                    std::string op = key.substr(0, key.find("_applied"));
                    if (verbose_) {
                        fprintf(stderr, "[morphect] │    %s: %d transformations\n", op.c_str(), val);
                    }
                    transformations += val;
                    pass_stats_["MBA_" + op] = val;

                    // Record for per-function summary
                    TransformationRecord rec;
                    rec.function_name = "<all>";
                    rec.pass_name = "MBA";
                    rec.operation = op;
                    for (int i = 0; i < val; i++) {
                        transformations_.push_back(rec);
                    }
                }
            }
        }

        logPassEnd("MBA", transformations);
        return result;
    }

    /**
     * Apply CFF Pass with detailed logging
     */
    std::vector<std::string> applyCFFPass(const std::vector<std::string>& lines,
                                           const std::vector<FunctionInfo>& functions) {
        logPassStart("CFF (Control Flow Flattening)");

        cff::LLVMCFFPass cff_pass;
        cff::CFFConfig cff_config;
        cff_config.enabled = true;
        cff_config.probability = global_probability_;
        cff_config.shuffle_states = true;
        cff_config.add_bogus_cases = true;
        cff_pass.setCFFConfig(cff_config);

        std::vector<std::string> result = lines;
        auto transform_result = cff_pass.transformIR(result);

        int transformations = 0;
        if (transform_result == TransformResult::Success) {
            auto stats = cff_pass.getStatistics();
            for (const auto& [key, val] : stats) {
                if (val > 0) {
                    logTransformation("<multiple>", "CFF", key,
                        std::to_string(val) + " " + key);
                    transformations += val;
                }
            }
        }

        logPassEnd("CFF", transformations);
        return result;
    }

    /**
     * Apply Bogus Control Flow Pass
     */
    std::vector<std::string> applyBogusPass(const std::vector<std::string>& lines,
                                             const std::vector<FunctionInfo>& functions) {
        logPassStart("Bogus Control Flow");

        cff::LLVMBogusPass bogus_pass;
        cff::BogusConfig bogus_config;
        bogus_config.enabled = true;
        bogus_config.probability = global_probability_;
        bogus_config.generate_dead_code = true;
        bogus_config.use_real_variables = true;
        bogus_pass.setBogusConfig(bogus_config);

        std::vector<std::string> result = lines;
        auto transform_result = bogus_pass.transformIR(result);

        int transformations = 0;
        if (transform_result == TransformResult::Success) {
            auto stats = bogus_pass.getStatistics();
            for (const auto& [key, val] : stats) {
                if (val > 0) {
                    logTransformation("<multiple>", "Bogus", key,
                        std::to_string(val) + " " + key);
                    transformations += val;
                }
            }
        }

        logPassEnd("Bogus CF", transformations);
        return result;
    }

    /**
     * Apply Variable Splitting Pass
     */
    std::vector<std::string> applyVariableSplitPass(const std::vector<std::string>& lines,
                                                     const std::vector<FunctionInfo>& functions) {
        logPassStart("Variable Splitting");

        data::LLVMVariableSplittingPass split_pass;
        data::VariableSplittingConfig split_config;
        split_config.enabled = true;
        split_config.probability = global_probability_;
        split_config.split_phi_nodes = true;
        split_config.max_splits_per_function = 5;
        split_config.exclude_patterns = {
            "%_op_", "%_cff_", "%_dead", "%split_", "%reconst_", "%_arith", "%mba_"
        };
        split_pass.configure(split_config);

        std::vector<std::string> result = lines;
        auto transform_result = split_pass.transformIR(result);

        int transformations = 0;
        if (transform_result == TransformResult::Success) {
            auto stats = split_pass.getStatistics();
            for (const auto& [key, val] : stats) {
                if (val > 0) {
                    logTransformation("<multiple>", "VarSplit", key,
                        std::to_string(val) + " " + key);
                    transformations += val;
                }
            }
        }

        logPassEnd("Variable Splitting", transformations);
        return result;
    }

    /**
     * Apply String Encoding Pass
     */
    std::vector<std::string> applyStringEncodingPass(const std::vector<std::string>& lines,
                                                      const std::vector<FunctionInfo>& functions) {
        logPassStart("String Encoding");

        data::LLVMStringEncodingPass str_pass;
        data::StringEncodingConfig str_config;
        str_config.enabled = true;
        str_config.method = data::StringEncodingMethod::XOR;
        str_config.min_string_length = 3;
        str_pass.configure(str_config);

        std::vector<std::string> result = lines;
        auto transform_result = str_pass.transformIR(result);

        int transformations = 0;
        if (transform_result == TransformResult::Success) {
            auto stats = str_pass.getStatistics();
            for (const auto& [key, val] : stats) {
                if (val > 0) {
                    logTransformation("<global>", "StrEnc", key,
                        std::to_string(val) + " " + key);
                    transformations += val;
                }
            }
        }

        logPassEnd("String Encoding", transformations);
        return result;
    }

    /**
     * Apply Dead Code Insertion Pass
     */
    std::vector<std::string> applyDeadCodePass(const std::vector<std::string>& lines,
                                                const std::vector<FunctionInfo>& functions) {
        logPassStart("Dead Code Insertion");

        deadcode::LLVMDeadCodePass dead_pass;
        deadcode::DeadCodeConfig dead_config;
        dead_config.enabled = true;
        dead_config.probability = global_probability_;
        dead_config.max_blocks = 5;
        dead_pass.setDeadCodeConfig(dead_config);

        std::vector<std::string> result = lines;
        auto transform_result = dead_pass.transformIR(result);

        int transformations = 0;
        if (transform_result == TransformResult::Success) {
            auto stats = dead_pass.getStatistics();
            for (const auto& [key, val] : stats) {
                if (val > 0) {
                    logTransformation("<multiple>", "DeadCode", key,
                        std::to_string(val) + " " + key);
                    transformations += val;
                }
            }
        }

        logPassEnd("Dead Code", transformations);
        return result;
    }

    /**
     * Count specific operations in lines
     */
    int countOperations(const std::vector<std::string>& lines, const std::string& op) {
        int count = 0;
        std::regex pattern("=\\s*" + op + "\\s+");
        for (const auto& line : lines) {
            if (std::regex_search(line, pattern)) {
                count++;
            }
        }
        return count;
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

    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }

    void parseConfig(const JsonValue& json) {
        if (json.has("global_probability")) {
            global_probability_ = json["global_probability"].asDouble(0.85);
        }

        const JsonValue& settings = json.has("obfuscation_settings")
            ? json["obfuscation_settings"] : json;

        if (settings.has("global_probability")) {
            global_probability_ = settings["global_probability"].asDouble(0.85);
        }

        if (json.has("control_flow")) {
            const auto& cf = json["control_flow"];
            if (cf.has("cff_enabled")) enable_cff_ = cf["cff_enabled"].asBool(false);
            if (cf.has("bogus_cf_enabled")) enable_bogus_ = cf["bogus_cf_enabled"].asBool(false);
        }

        if (json.has("data_obfuscation")) {
            const auto& data = json["data_obfuscation"];
            if (data.has("variable_splitting")) enable_var_split_ = data["variable_splitting"].asBool(false);
            if (data.has("string_encoding")) enable_string_enc_ = data["string_encoding"].asBool(false);
        }

        if (json.has("dead_code")) {
            const auto& dc = json["dead_code"];
            if (dc.has("enabled")) enable_dead_code_ = dc["enabled"].asBool(false);
        }

        logger_.info("Loaded configuration");
        logger_.info("  Probability: {:.0f}%", global_probability_ * 100);
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
    std::cout << "  --mba                 Enable MBA (Mixed Boolean Arithmetic)" << std::endl;
    std::cout << "  --cff                 Enable Control Flow Flattening" << std::endl;
    std::cout << "  --bogus               Enable Bogus Control Flow" << std::endl;
    std::cout << "  --varsplit            Enable Variable Splitting" << std::endl;
    std::cout << "  --strenc              Enable String Encoding" << std::endl;
    std::cout << "  --deadcode            Enable Dead Code Insertion" << std::endl;
    std::cout << "  --all                 Enable all obfuscation passes" << std::endl;
    std::cout << "  --verbose, -v         Enable verbose output (show each transformation)" << std::endl;
    std::cout << "  --help, -h            Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Workflow:" << std::endl;
    std::cout << "  1. clang -S -emit-llvm -O0 source.c -o source.ll" << std::endl;
    std::cout << "  2. " << program << " --mba --verbose source.ll obfuscated.ll" << std::endl;
    std::cout << "  3. llc obfuscated.ll -o obfuscated.s" << std::endl;
    std::cout << "  4. clang obfuscated.s -o output" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program << " --mba input.ll output.ll                # MBA only" << std::endl;
    std::cout << "  " << program << " --mba --cff input.ll output.ll          # MBA + CFF" << std::endl;
    std::cout << "  " << program << " --all --probability 0.5 in.ll out.ll    # All passes, 50%%" << std::endl;
    std::cout << "  " << program << " -v --mba input.ll output.ll             # Verbose MBA" << std::endl;
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
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (input_file.empty() || output_file.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    // Check if any pass is enabled
    if (!enable_mba && !enable_cff && !enable_bogus &&
        !enable_varsplit && !enable_strenc && !enable_deadcode) {
        std::cerr << "Error: No obfuscation passes enabled." << std::endl;
        std::cerr << "Use --mba, --cff, --bogus, --varsplit, --strenc, --deadcode, or --all" << std::endl;
        return 1;
    }

    // Set up logging
    if (verbose) {
        LogConfig::get().setLevel(LogLevel::Debug);
    }

    printBanner();

    // Create obfuscator
    LLVMIRObfuscator obfuscator;
    obfuscator.setVerbose(verbose);

    // Load config
    if (!config_file.empty()) {
        if (!obfuscator.loadConfig(config_file)) {
            return 1;
        }
    }

    if (probability >= 0) {
        obfuscator.setGlobalProbability(probability);
    }

    // Apply command-line pass enables
    obfuscator.setEnableMBA(enable_mba);
    obfuscator.setEnableCFF(enable_cff);
    obfuscator.setEnableBogus(enable_bogus);
    obfuscator.setEnableVariableSplit(enable_varsplit);
    obfuscator.setEnableStringEncoding(enable_strenc);
    obfuscator.setEnableDeadCode(enable_deadcode);

    // Print enabled passes
    fprintf(stderr, "[morphect] Input: %s\n", input_file.c_str());
    fprintf(stderr, "[morphect] Output: %s\n", output_file.c_str());
    fprintf(stderr, "[morphect] Probability: %.0f%%\n", (probability >= 0 ? probability : 0.85) * 100);
    fprintf(stderr, "[morphect] Passes: ");
    std::vector<std::string> enabled_passes;
    if (enable_mba) enabled_passes.push_back("MBA");
    if (enable_cff) enabled_passes.push_back("CFF");
    if (enable_bogus) enabled_passes.push_back("Bogus");
    if (enable_varsplit) enabled_passes.push_back("VarSplit");
    if (enable_strenc) enabled_passes.push_back("StrEnc");
    if (enable_deadcode) enabled_passes.push_back("DeadCode");
    for (size_t i = 0; i < enabled_passes.size(); i++) {
        fprintf(stderr, "%s%s", enabled_passes[i].c_str(),
                i < enabled_passes.size() - 1 ? ", " : "");
    }
    fprintf(stderr, "\n\n");

    // Read input
    std::ifstream input(input_file);
    if (!input.is_open()) {
        std::cerr << "[morphect] Error: Cannot open input file: " << input_file << std::endl;
        return 1;
    }

    std::string ir_code((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    input.close();

    fprintf(stderr, "[morphect] Read %zu bytes from %s\n", ir_code.size(), input_file.c_str());

    // Obfuscate
    std::string obfuscated = obfuscator.obfuscateFull(ir_code);

    // Write output
    std::ofstream output(output_file);
    if (!output.is_open()) {
        std::cerr << "[morphect] Error: Cannot create output file: " << output_file << std::endl;
        return 1;
    }

    output << obfuscated;
    output.close();

    fprintf(stderr, "[morphect] Wrote %zu bytes to %s\n", obfuscated.size(), output_file.c_str());

    // Size statistics
    double increase = ((double)obfuscated.size() / ir_code.size() - 1.0) * 100.0;
    fprintf(stderr, "[morphect] Size change: %+.1f%%\n", increase);

    // Print transformation statistics
    obfuscator.printStatistics();

    return 0;
}
