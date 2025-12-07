/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * assembly_obfuscator.cpp - x86/x64 Assembly Obfuscator (Standalone Tool)
 *
 * This is a standalone tool that reads x86/x64 assembly files,
 * applies obfuscation transformations, and outputs obfuscated assembly.
 *
 * Usage:
 *   morphect-asm --config config.json input.s output.s
 *
 * Workflow:
 *   1. gcc -S -masm=intel -O0 source.c -o source.s
 *   2. morphect-asm --config config.json source.s obfuscated.s
 *   3. gcc obfuscated.s -o output
 */

#include "morphect.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>
#include <map>

using namespace morphect;

/**
 * Assembly Obfuscator class
 */
class AssemblyObfuscator {
public:
    AssemblyObfuscator() : logger_("ASMObfuscator") {}

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

    std::string detectArchitecture(const std::string& code) {
        // Check for 64-bit registers
        std::regex x64_re(R"(\b(?:rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|r[89]|r1[0-5])\b)");
        if (std::regex_search(code, x64_re)) {
            return "x86_64";
        }

        // Check for 32-bit registers
        std::regex x32_re(R"(\b(?:eax|ebx|ecx|edx|esi|edi|ebp|esp)\b)");
        if (std::regex_search(code, x32_re)) {
            return "x86_32";
        }

        return "x86_64";  // Default
    }

    std::string obfuscate(const std::string& asm_code) {
        detected_arch_ = detectArchitecture(asm_code);
        logger_.info("Detected architecture: {}", detected_arch_);

        std::istringstream input(asm_code);
        std::vector<std::string> result;
        std::string line;
        int line_count = 0;

        while (std::getline(input, line)) {
            line_count++;
            std::string trimmed = trim(line);

            // Skip labels, directives, comments
            if (shouldSkipLine(trimmed)) {
                result.push_back(line);
                continue;
            }

            // Currently, pattern-based transformation is disabled
            // as it can break register allocation.
            // Future: Implement safe transformations
            result.push_back(line);
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
        logger_.info("=== Assembly Obfuscation Statistics ===");
        logger_.info("Detected architecture: {}", detected_arch_);

        int total = 0;
        for (const auto& [name, count] : stats_.getIntStats()) {
            logger_.info("  {}: {}", name, count);
            total += count;
        }

        logger_.info("Total transformations: {}", total);
        logger_.info("========================================");
    }

private:
    Logger logger_;
    Statistics stats_;
    std::string detected_arch_ = "unknown";
    double global_probability_ = 0.7;

    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }

    bool shouldSkipLine(const std::string& trimmed) {
        if (trimmed.empty()) return true;
        if (trimmed[0] == '.') return true;   // Directive
        if (trimmed[0] == '#') return true;   // Comment
        if (trimmed[0] == ';') return true;   // Comment
        if (trimmed.find(':') != std::string::npos) return true;  // Label

        return false;
    }

    void parseConfig(const JsonValue& json) {
        if (json.has("global_probability")) {
            global_probability_ = json["global_probability"].asDouble(0.7);
        }

        const JsonValue& settings = json.has("obfuscation_settings")
            ? json["obfuscation_settings"]
            : json;

        if (settings.has("global_probability")) {
            global_probability_ = settings["global_probability"].asDouble(0.7);
        }

        logger_.info("Loaded configuration");
    }
};

/**
 * Print usage
 */
void printUsage(const char* program) {
    std::cout << getBanner() << std::endl;
    std::cout << "Usage: " << program << " [options] <input.s> <output.s>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --config <file>       Configuration file (JSON)" << std::endl;
    std::cout << "  --probability <n>     Global transformation probability (0.0-1.0)" << std::endl;
    std::cout << "  --verbose             Enable verbose output" << std::endl;
    std::cout << "  --help                Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Workflow:" << std::endl;
    std::cout << "  1. gcc -S -masm=intel -O0 source.c -o source.s" << std::endl;
    std::cout << "  2. " << program << " --config config.json source.s obfuscated.s" << std::endl;
    std::cout << "  3. gcc obfuscated.s -o output" << std::endl;
    std::cout << std::endl;
    std::cout << "NOTE: Assembly-level obfuscation is currently limited." << std::endl;
    std::cout << "      Prefer using the GCC plugin or IR obfuscator." << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file;
    std::string input_file;
    std::string output_file;
    double probability = -1;
    bool verbose = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--probability" && i + 1 < argc) {
            probability = std::stod(argv[++i]);
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
    AssemblyObfuscator obfuscator;

    // Load config
    if (!config_file.empty()) {
        if (!obfuscator.loadConfig(config_file)) {
            return 1;
        }
    }

    if (probability >= 0) {
        obfuscator.setGlobalProbability(probability);
    }

    // Read input
    std::ifstream input(input_file);
    if (!input.is_open()) {
        LOG_ERROR("Cannot open input file: {}", input_file);
        return 1;
    }

    std::string asm_code((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
    input.close();

    LOG_INFO("Read {} bytes from {}", asm_code.size(), input_file);

    // Obfuscate
    std::string obfuscated = obfuscator.obfuscate(asm_code);

    // Write output
    std::ofstream output(output_file);
    if (!output.is_open()) {
        LOG_ERROR("Cannot create output file: {}", output_file);
        return 1;
    }

    output << obfuscated;
    output.close();

    LOG_INFO("Wrote {} bytes to {}", obfuscated.size(), output_file);

    obfuscator.printStatistics();

    return 0;
}
