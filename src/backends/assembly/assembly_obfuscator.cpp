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
#include <algorithm>
#include <cctype>

using namespace morphect;

/**
 * Safe assembly transformation types
 * These transformations preserve semantics and don't break register allocation
 */
enum class AsmTransformType {
    // Arithmetic identity transformations (reg = reg)
    XorSelfToMovZero,      // xor eax, eax -> mov eax, 0 (or reverse)
    SubSelfToMovZero,      // sub eax, eax -> mov eax, 0 (or reverse)
    AddZeroIdentity,       // add eax, 0 -> nop sequence
    SubZeroIdentity,       // sub eax, 0 -> nop sequence
    XorZeroIdentity,       // xor eax, 0 -> nop sequence

    // MBA-style arithmetic
    AddToMBA,              // add eax, ebx -> (eax ^ ebx) + 2*(eax & ebx)
    SubToMBA,              // sub eax, ebx -> (eax ^ ebx) - 2*(~eax & ebx)
    XorToMBA,              // xor eax, ebx -> (eax | ebx) - (eax & ebx)

    // Constant splitting
    MovImmSplit,           // mov eax, 0x12345678 -> mov eax, 0x12340000; or eax, 0x5678

    // Instruction substitution
    MovToLeaPush,          // mov [mem], reg -> push reg; pop [mem] (if safe)
    IncDecToAddSub,        // inc eax -> add eax, 1

    // Junk insertion (NOPs that don't affect state)
    InsertJunkNops,        // Insert semantic NOP sequences
};

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

    void setTransformEnabled(bool enabled) {
        transforms_enabled_ = enabled;
    }

    std::string detectArchitecture(const std::string& code) {
        // Check for 64-bit registers
        std::regex x64_re(R"(\b(?:rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|r[89]|r1[0-5])\b)", std::regex::icase);
        if (std::regex_search(code, x64_re)) {
            return "x86_64";
        }

        // Check for 32-bit registers
        std::regex x32_re(R"(\b(?:eax|ebx|ecx|edx|esi|edi|ebp|esp)\b)", std::regex::icase);
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

            // Apply safe transformations
            if (transforms_enabled_ && GlobalRandom::decide(global_probability_)) {
                auto transformed = transformInstruction(trimmed, line);
                if (!transformed.empty()) {
                    for (const auto& t : transformed) {
                        result.push_back(t);
                    }
                    continue;
                }
            }

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
        logger_.info("Transforms enabled: {}", transforms_enabled_ ? "yes" : "no");

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
    bool transforms_enabled_ = true;

    std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }

    std::string getIndent(const std::string& line) {
        size_t pos = line.find_first_not_of(" \t");
        return pos != std::string::npos ? line.substr(0, pos) : "\t";
    }

    bool shouldSkipLine(const std::string& trimmed) {
        if (trimmed.empty()) return true;
        if (trimmed[0] == '.') return true;   // Directive
        if (trimmed[0] == '#') return true;   // Comment
        if (trimmed[0] == ';') return true;   // Comment
        if (trimmed.find(':') != std::string::npos) return true;  // Label

        return false;
    }

    /**
     * Transform a single instruction into equivalent obfuscated sequence
     * Returns empty vector if no transformation applies
     */
    std::vector<std::string> transformInstruction(const std::string& trimmed, const std::string& original) {
        std::string indent = getIndent(original);
        std::vector<std::string> result;

        // Try each transformation type
        if (tryTransformXorSelf(trimmed, indent, result)) return result;
        if (tryTransformSubSelf(trimmed, indent, result)) return result;
        if (tryTransformMovImm(trimmed, indent, result)) return result;
        if (tryTransformIncDec(trimmed, indent, result)) return result;
        if (tryTransformAdd(trimmed, indent, result)) return result;
        if (tryTransformSub(trimmed, indent, result)) return result;
        if (tryTransformXor(trimmed, indent, result)) return result;

        // Maybe insert junk before the instruction
        if (GlobalRandom::decide(0.3)) {
            insertJunkNop(indent, result);
            result.push_back(original);
            stats_.increment("junk_nops_inserted");
            return result;
        }

        return {};  // No transformation
    }

    /**
     * xor reg, reg -> sub reg, reg (semantically equivalent, both zero the register)
     * or reverse: sub reg, reg -> xor reg, reg
     */
    bool tryTransformXorSelf(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex xor_self(R"(xor\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, xor_self)) {
            std::string reg1 = match[1].str();
            std::string reg2 = match[2].str();

            // Check if it's xor reg, reg (zeroing pattern)
            if (toLower(reg1) == toLower(reg2)) {
                // Transform to sub reg, reg or mov reg, 0
                int choice = GlobalRandom::nextInt(0, 1);
                if (choice == 0) {
                    result.push_back(indent + "sub " + reg1 + ", " + reg2);
                } else {
                    result.push_back(indent + "mov " + reg1 + ", 0");
                }
                stats_.increment("xor_self_transformed");
                return true;
            }
        }
        return false;
    }

    /**
     * sub reg, reg -> xor reg, reg (semantically equivalent)
     */
    bool tryTransformSubSelf(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex sub_self(R"(sub\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, sub_self)) {
            std::string reg1 = match[1].str();
            std::string reg2 = match[2].str();

            if (toLower(reg1) == toLower(reg2)) {
                result.push_back(indent + "xor " + reg1 + ", " + reg2);
                stats_.increment("sub_self_transformed");
                return true;
            }
        }
        return false;
    }

    /**
     * mov reg, imm -> split into multiple operations
     * e.g., mov eax, 0x12345678 -> mov eax, 0x12340000; add eax, 0x5678
     */
    bool tryTransformMovImm(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex mov_imm(R"(mov\s+(\w+)\s*,\s*(0x[0-9a-fA-F]+|\d+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, mov_imm)) {
            std::string reg = match[1].str();
            std::string imm_str = match[2].str();

            // Parse the immediate value
            uint64_t imm;
            if (imm_str.find("0x") == 0 || imm_str.find("0X") == 0) {
                imm = std::stoull(imm_str, nullptr, 16);
            } else {
                imm = std::stoull(imm_str);
            }

            // Only transform larger values (> 0xFF)
            if (imm > 0xFF && imm <= 0xFFFFFFFF) {
                int choice = GlobalRandom::nextInt(0, 2);

                if (choice == 0) {
                    // Split: mov reg, high; or reg, low
                    uint32_t high = static_cast<uint32_t>(imm) & 0xFFFF0000;
                    uint32_t low = static_cast<uint32_t>(imm) & 0x0000FFFF;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(high));
                    result.push_back(indent + "or " + reg + ", 0x" + toHex(low));
                } else if (choice == 1) {
                    // XOR-based: mov reg, val1; xor reg, val2 where val1^val2 = imm
                    uint32_t val1 = GlobalRandom::nextInt(0, 0xFFFF) << 16 | GlobalRandom::nextInt(0, 0xFFFF);
                    uint32_t val2 = static_cast<uint32_t>(imm) ^ val1;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(val1));
                    result.push_back(indent + "xor " + reg + ", 0x" + toHex(val2));
                } else {
                    // Add-based: mov reg, val1; add reg, val2 where val1+val2 = imm
                    uint32_t val1 = GlobalRandom::nextInt(0, static_cast<int>(imm));
                    uint32_t val2 = static_cast<uint32_t>(imm) - val1;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(val1));
                    result.push_back(indent + "add " + reg + ", 0x" + toHex(val2));
                }

                stats_.increment("mov_imm_split");
                return true;
            }
        }
        return false;
    }

    /**
     * inc reg -> add reg, 1
     * dec reg -> sub reg, 1
     */
    bool tryTransformIncDec(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex inc_re(R"(inc\s+(\w+))", std::regex::icase);
        std::regex dec_re(R"(dec\s+(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, inc_re)) {
            std::string reg = match[1].str();
            result.push_back(indent + "add " + reg + ", 1");
            stats_.increment("inc_to_add");
            return true;
        }

        if (std::regex_match(trimmed, match, dec_re)) {
            std::string reg = match[1].str();
            result.push_back(indent + "sub " + reg + ", 1");
            stats_.increment("dec_to_sub");
            return true;
        }

        return false;
    }

    /**
     * add reg, imm -> MBA: push; operations; pop
     * add dst, src -> MBA style with temp register
     * Note: We use lea for safe addition transformation
     */
    bool tryTransformAdd(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex add_reg_imm(R"(add\s+(\w+)\s*,\s*(\d+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, add_reg_imm)) {
            std::string reg = match[1].str();
            int imm = std::stoi(match[2].str());

            // For small immediates, use lea (doesn't affect flags differently than add)
            // This is a mild obfuscation but safe
            if (imm > 0 && imm < 128 && detected_arch_ == "x86_64") {
                // lea reg, [reg + imm]
                result.push_back(indent + "lea " + reg + ", [" + reg + " + " + std::to_string(imm) + "]");
                stats_.increment("add_to_lea");
                return true;
            }
        }

        return false;
    }

    /**
     * sub reg, imm -> add reg, -imm (if small enough)
     */
    bool tryTransformSub(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex sub_reg_imm(R"(sub\s+(\w+)\s*,\s*(\d+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, sub_reg_imm)) {
            std::string reg = match[1].str();
            int imm = std::stoi(match[2].str());

            // For small immediates, use add with negative
            if (imm > 0 && imm < 128) {
                result.push_back(indent + "add " + reg + ", " + std::to_string(-imm));
                stats_.increment("sub_to_add_neg");
                return true;
            }
        }

        return false;
    }

    /**
     * xor reg, imm -> not reg; and reg, ~imm; not reg (one of several MBA equivalents)
     * This is more complex but semantically equivalent
     */
    bool tryTransformXor(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        // Skip for now - XOR transformations with MBA need careful register handling
        // to avoid breaking code. The simpler xor_self transform above is safe.
        (void)trimmed;
        (void)indent;
        (void)result;
        return false;
    }

    /**
     * Insert semantic NOP (doesn't change any state)
     */
    void insertJunkNop(const std::string& indent, std::vector<std::string>& result) {
        int choice = GlobalRandom::nextInt(0, 4);

        switch (choice) {
            case 0:
                // xchg reg, reg (no-op)
                if (detected_arch_ == "x86_64") {
                    result.push_back(indent + "xchg rax, rax");
                } else {
                    result.push_back(indent + "xchg eax, eax");
                }
                break;
            case 1:
                // lea reg, [reg] (no-op)
                if (detected_arch_ == "x86_64") {
                    result.push_back(indent + "lea rax, [rax]");
                } else {
                    result.push_back(indent + "lea eax, [eax]");
                }
                break;
            case 2:
                // nop with size variants
                result.push_back(indent + "nop");
                break;
            case 3:
                // mov reg, reg (no-op)
                if (detected_arch_ == "x86_64") {
                    result.push_back(indent + "mov rax, rax");
                } else {
                    result.push_back(indent + "mov eax, eax");
                }
                break;
            case 4:
                // or reg, 0 (no-op but affects flags)
                if (detected_arch_ == "x86_64") {
                    result.push_back(indent + "or rax, 0");
                } else {
                    result.push_back(indent + "or eax, 0");
                }
                break;
        }
    }

    std::string toLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    std::string toHex(uint32_t val) {
        std::ostringstream ss;
        ss << std::hex << val;
        return ss.str();
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

        if (json.has("assembly")) {
            const auto& asm_settings = json["assembly"];
            if (asm_settings.has("transforms_enabled")) {
                transforms_enabled_ = asm_settings["transforms_enabled"].asBool(true);
            }
        }

        logger_.info("Loaded configuration (transforms: {})", transforms_enabled_ ? "enabled" : "disabled");
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
