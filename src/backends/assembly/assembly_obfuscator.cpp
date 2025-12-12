/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * assembly_obfuscator.cpp - x86/x64 Assembly Obfuscator (Standalone Tool)
 *
 * Features:
 *   - Control flow obfuscation (opaque predicates, bogus branches)
 *   - Constant obfuscation (splitting, MBA)
 *   - String encryption
 *   - Dead code insertion with real computations
 *   - Instruction substitution
 *   - Label randomization
 *   - Function prologue obfuscation
 *
 * Usage:
 *   morphect-asm --config config.json input.s output.s
 */

#include "morphect.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <optional>
#include <iomanip>

using namespace morphect;

/**
 * Configuration for assembly obfuscation
 */
struct AsmObfConfig {
    double global_probability = 0.7;
    bool transforms_enabled = true;

    // Feature toggles
    bool enable_control_flow = true;  // TESTING
    bool enable_constant_obfuscation = true;
    bool enable_string_encryption = false;  // Disabled: needs runtime decryption support
    bool enable_dead_code = true;  // Enabled
    bool enable_mba = true;  // Enabled
    bool enable_label_randomization = true;
    bool enable_prologue_obfuscation = true;  // Enabled

    // Probabilities for various transforms
    double opaque_predicate_prob = 0.15;
    double bogus_branch_prob = 0.15;
    double constant_split_prob = 0.4;
    double mba_prob = 0.0;
    double dead_code_prob = 0.0;
    double junk_nop_prob = 0.0;
};

/**
 * Assembly Obfuscator class - Full featured version
 */
class AssemblyObfuscator {
public:
    AssemblyObfuscator() : logger_("ASMObfuscator") {
        initializeRegisterMaps();
    }

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
        config_.global_probability = prob;
    }

    void setTransformEnabled(bool enabled) {
        config_.transforms_enabled = enabled;
    }

    std::string detectArchitecture(const std::string& code) {
        std::regex x64_re(R"(\b(?:rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|r[89]|r1[0-5])\b)", std::regex::icase);
        if (std::regex_search(code, x64_re)) {
            return "x86_64";
        }
        std::regex x32_re(R"(\b(?:eax|ebx|ecx|edx|esi|edi|ebp|esp)\b)", std::regex::icase);
        if (std::regex_search(code, x32_re)) {
            return "x86_32";
        }
        return "x86_64";
    }

    std::string obfuscate(const std::string& asm_code) {
        detected_arch_ = detectArchitecture(asm_code);
        logger_.info("Detected architecture: {}", detected_arch_);

        // Parse into lines
        std::istringstream input(asm_code);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input, line)) {
            lines.push_back(line);
        }

        // Pass 1: Collect string literals and labels
        collectStrings(lines);
        collectLabels(lines);

        // Pass 2: Randomize labels if enabled
        if (config_.enable_label_randomization) {
            randomizeLabels(lines);
        }

        // Pass 3: Encrypt strings if enabled
        if (config_.enable_string_encryption) {
            encryptStrings(lines);
        }

        // Pass 4: Main transformation pass
        std::vector<std::string> result;
        bool in_function = false;
        bool after_prologue = false;

        for (size_t i = 0; i < lines.size(); i++) {
            std::string trimmed = trim(lines[i]);

            // Track function boundaries
            if (trimmed.find(".cfi_startproc") != std::string::npos) {
                in_function = true;
                after_prologue = false;
                result.push_back(lines[i]);
                continue;
            }
            if (trimmed.find(".cfi_endproc") != std::string::npos) {
                in_function = false;
                result.push_back(lines[i]);
                continue;
            }

            // Detect end of prologue (after mov rbp, rsp or first real instruction)
            if (in_function && !after_prologue) {
                if (trimmed.find("mov") == 0 && trimmed.find("rbp") != std::string::npos &&
                    trimmed.find("rsp") != std::string::npos) {
                    result.push_back(lines[i]);
                    after_prologue = true;

                    // Insert prologue obfuscation after frame setup
                    if (config_.enable_prologue_obfuscation && GlobalRandom::decide(0.3)) {
                        insertPrologueObfuscation(getIndent(lines[i]), result);
                    }
                    continue;
                }
            }

            // Skip labels, directives, comments
            if (shouldSkipLine(trimmed)) {
                result.push_back(lines[i]);
                continue;
            }

            // Check lookahead for flag usage FIRST - we need this for control flow decisions
            bool next_uses_flags = checkNextUsesFlags(lines, i);
            bool current_uses_flags = usesFlags(trimmed);
            bool current_sets_flags = setsFlags(trimmed);

            // Check for control flow opportunities
            // IMPORTANT: Don't insert before instructions that USE flags (they depend on previous cmp/test)
            // ALSO: Don't insert AFTER instructions that SET flags if next instruction USES flags
            // Because our control flow obfuscation would clobber flags between SET and USE
            bool safe_for_control_flow = !current_uses_flags && !(current_sets_flags && next_uses_flags);
            if (in_function && after_prologue && config_.enable_control_flow && safe_for_control_flow) {
                // Insert opaque predicate before some instructions
                if (!isControlFlow(trimmed) && GlobalRandom::decide(config_.opaque_predicate_prob)) {
                    insertOpaquePredicate(getIndent(lines[i]), result);
                    stats_.increment("opaque_predicates");
                }

                // Insert bogus conditional branch
                if (!isControlFlow(trimmed) && GlobalRandom::decide(config_.bogus_branch_prob)) {
                    insertBogusBranch(getIndent(lines[i]), result);
                    stats_.increment("bogus_branches");
                }
            }

            // Never transform control flow instructions themselves
            if (isControlFlow(trimmed)) {
                result.push_back(lines[i]);
                continue;
            }

            // Apply transformations
            if (config_.transforms_enabled && GlobalRandom::decide(config_.global_probability)) {
                auto transformed = transformInstruction(trimmed, lines[i], next_uses_flags, in_function && after_prologue);
                if (!transformed.empty()) {
                    for (const auto& t : transformed) {
                        result.push_back(t);
                    }
                    continue;
                }
            }

            result.push_back(lines[i]);
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
    AsmObfConfig config_;
    std::string detected_arch_ = "unknown";

    // Label tracking
    std::map<std::string, std::string> label_map_;  // original -> randomized
    std::set<std::string> local_labels_;
    int label_counter_ = 0;

    // String tracking
    struct StringInfo {
        std::string label;
        std::string content;
        size_t line_num;
    };
    std::vector<StringInfo> strings_;

    // Register mappings
    std::map<std::string, std::string> reg_to_parent64_;
    std::set<std::string> all_registers_;

    void initializeRegisterMaps() {
        reg_to_parent64_["rax"] = "rax"; reg_to_parent64_["eax"] = "rax";
        reg_to_parent64_["ax"] = "rax"; reg_to_parent64_["al"] = "rax"; reg_to_parent64_["ah"] = "rax";
        reg_to_parent64_["rbx"] = "rbx"; reg_to_parent64_["ebx"] = "rbx";
        reg_to_parent64_["bx"] = "rbx"; reg_to_parent64_["bl"] = "rbx"; reg_to_parent64_["bh"] = "rbx";
        reg_to_parent64_["rcx"] = "rcx"; reg_to_parent64_["ecx"] = "rcx";
        reg_to_parent64_["cx"] = "rcx"; reg_to_parent64_["cl"] = "rcx"; reg_to_parent64_["ch"] = "rcx";
        reg_to_parent64_["rdx"] = "rdx"; reg_to_parent64_["edx"] = "rdx";
        reg_to_parent64_["dx"] = "rdx"; reg_to_parent64_["dl"] = "rdx"; reg_to_parent64_["dh"] = "rdx";
        reg_to_parent64_["rsi"] = "rsi"; reg_to_parent64_["esi"] = "rsi";
        reg_to_parent64_["si"] = "rsi"; reg_to_parent64_["sil"] = "rsi";
        reg_to_parent64_["rdi"] = "rdi"; reg_to_parent64_["edi"] = "rdi";
        reg_to_parent64_["di"] = "rdi"; reg_to_parent64_["dil"] = "rdi";
        reg_to_parent64_["rbp"] = "rbp"; reg_to_parent64_["ebp"] = "rbp";
        reg_to_parent64_["bp"] = "rbp"; reg_to_parent64_["bpl"] = "rbp";
        reg_to_parent64_["rsp"] = "rsp"; reg_to_parent64_["esp"] = "rsp";
        reg_to_parent64_["sp"] = "rsp"; reg_to_parent64_["spl"] = "rsp";
        for (int i = 8; i <= 15; i++) {
            std::string base = "r" + std::to_string(i);
            reg_to_parent64_[base] = base;
            reg_to_parent64_[base + "d"] = base;
            reg_to_parent64_[base + "w"] = base;
            reg_to_parent64_[base + "b"] = base;
        }
        for (const auto& [reg, _] : reg_to_parent64_) {
            all_registers_.insert(reg);
        }
    }

    // ==================== Label Collection & Randomization ====================

    void collectLabels(const std::vector<std::string>& lines) {
        std::regex label_re(R"(^\.?(L[A-Za-z0-9_]+):)");
        for (const auto& line : lines) {
            std::smatch match;
            std::string trimmed = trim(line);
            if (std::regex_search(trimmed, match, label_re)) {
                std::string label = match[1].str();
                // Only randomize local labels (starting with .L)
                if (label[0] == 'L' || (trimmed[0] == '.' && label[0] == 'L')) {
                    local_labels_.insert(label);
                }
            }
        }
    }

    void randomizeLabels(std::vector<std::string>& lines) {
        // Generate random names for local labels
        for (const auto& label : local_labels_) {
            std::string random_name = generateRandomLabel();
            label_map_[label] = random_name;
            label_map_["." + label] = "." + random_name;
        }

        // Replace labels in all lines
        for (auto& line : lines) {
            for (const auto& [orig, repl] : label_map_) {
                size_t pos = 0;
                while ((pos = line.find(orig, pos)) != std::string::npos) {
                    // Make sure we're matching a complete label
                    bool is_word_boundary = true;
                    if (pos > 0 && (std::isalnum(line[pos-1]) || line[pos-1] == '_')) {
                        is_word_boundary = false;
                    }
                    size_t end_pos = pos + orig.length();
                    if (end_pos < line.length() && (std::isalnum(line[end_pos]) || line[end_pos] == '_')) {
                        is_word_boundary = false;
                    }

                    if (is_word_boundary) {
                        line.replace(pos, orig.length(), repl);
                        pos += repl.length();
                    } else {
                        pos++;
                    }
                }
            }
        }

        stats_.increment("labels_randomized", static_cast<int>(label_map_.size()));
    }

    std::string generateRandomLabel() {
        std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string label = "L";
        label += chars[GlobalRandom::nextInt(0, static_cast<int>(chars.size()) - 1)];
        for (int i = 0; i < 8; i++) {
            label += chars[GlobalRandom::nextInt(0, static_cast<int>(chars.size()) - 1)];
        }
        label += "_" + std::to_string(label_counter_++);
        return label;
    }

    // ==================== String Collection & Encryption ====================

    void collectStrings(const std::vector<std::string>& lines) {
        std::regex string_re(R"(^(\.LC\d+):)");
        std::regex ascii_re(R"delim(\s*\.string\s+"(.*)"|\.ascii\s+"(.*)")delim");

        for (size_t i = 0; i < lines.size(); i++) {
            std::smatch match;
            std::string trimmed = trim(lines[i]);
            if (std::regex_search(trimmed, match, string_re)) {
                std::string label = match[1].str();
                // Look for string content on next line
                if (i + 1 < lines.size()) {
                    std::smatch str_match;
                    if (std::regex_search(lines[i + 1], str_match, ascii_re)) {
                        std::string content = str_match[1].matched ? str_match[1].str() : str_match[2].str();
                        strings_.push_back({label, content, i});
                    }
                }
            }
        }
    }

    void encryptStrings(std::vector<std::string>& lines) {
        // For each string, XOR encrypt it and add decryption stub
        for (const auto& str_info : strings_) {
            if (str_info.content.length() < 4) continue;  // Skip very short strings

            uint8_t key = static_cast<uint8_t>(GlobalRandom::nextInt(1, 255));
            std::string encrypted;

            for (char c : str_info.content) {
                encrypted += static_cast<char>(static_cast<uint8_t>(c) ^ key);
            }

            // Convert to hex bytes
            std::ostringstream hex_bytes;
            hex_bytes << "\t.byte ";
            for (size_t i = 0; i < encrypted.size(); i++) {
                hex_bytes << "0x" << std::hex << std::setfill('0') << std::setw(2)
                          << (static_cast<unsigned int>(static_cast<uint8_t>(encrypted[i])));
                if (i < encrypted.size() - 1) hex_bytes << ", ";
            }
            hex_bytes << ", 0x00";  // Null terminator (XOR with key)

            // Replace the .string directive with encrypted bytes
            // And add a comment with the key for reference
            for (size_t i = str_info.line_num; i < lines.size() && i < str_info.line_num + 3; i++) {
                if (lines[i].find(".string") != std::string::npos ||
                    lines[i].find(".ascii") != std::string::npos) {
                    lines[i] = hex_bytes.str() + "  # XOR key: 0x" + toHex(key);
                    stats_.increment("strings_encrypted");
                    break;
                }
            }
        }
    }

    // ==================== Control Flow Obfuscation ====================

    void insertOpaquePredicate(const std::string& indent, std::vector<std::string>& result) {
        // Opaque predicate: condition that always evaluates the same way
        // but is hard to determine statically
        // CRITICAL: Use pushfq/popfq to preserve flags!
        // CRITICAL: Protect red zone - functions may use stack below RSP!
        // CRITICAL: Jump BEFORE pop - pop can modify flags!

        int choice = GlobalRandom::nextInt(0, 3);
        std::string skip_label = "." + generateRandomLabel();
        std::string trap_label = "." + generateRandomLabel();

        // Protect red zone (128 bytes below RSP on System V AMD64 ABI)
        // Use lea instead of sub to avoid affecting flags
        result.push_back(indent + "lea rsp, [rsp - 128]");

        // Save flags FIRST before any operations
        result.push_back(indent + "pushfq");

        switch (choice) {
            case 0: {
                // (x & x) == x is always true (AND with self is identity)
                // Also: any value AND itself equals itself
                result.push_back(indent + "# opaque predicate: (x & x) == x");
                result.push_back(indent + "push r11");
                result.push_back(indent + "push r10");
                result.push_back(indent + "mov r11, rsp");
                result.push_back(indent + "mov r10, r11");
                result.push_back(indent + "and r10, r11");  // r10 = r11 & r11 = r11
                result.push_back(indent + "cmp r10, r11");
                // Jump BEFORE pop
                result.push_back(indent + "jne " + trap_label);  // if not equal (never happens), trap
                result.push_back(indent + "pop r10");
                result.push_back(indent + "pop r11");
                result.push_back(indent + "jmp " + skip_label);
                result.push_back(trap_label + ":");
                result.push_back(indent + "pop r10");  // still need to clean stack
                result.push_back(indent + "pop r11");
                result.push_back(indent + ".byte 0xcc");
                result.push_back(skip_label + ":");
                break;
            }
            case 1: {
                // (x | 1) != 0 is always true
                result.push_back(indent + "# opaque predicate: (x|1) != 0");
                result.push_back(indent + "push r11");
                result.push_back(indent + "mov r11, 1");
                result.push_back(indent + "or r11, rsp");
                // Jump BEFORE pop - pop affects flags!
                result.push_back(indent + "jz " + trap_label);  // if zero (never happens), trap
                result.push_back(indent + "pop r11");
                result.push_back(indent + "jmp " + skip_label);
                result.push_back(trap_label + ":");
                result.push_back(indent + "pop r11");  // still need to clean stack
                result.push_back(indent + ".byte 0xf4");
                result.push_back(skip_label + ":");
                break;
            }
            case 2: {
                // (x & 1) == 0 is always true for RSP (stack is always 8/16-byte aligned)
                // Test that RSP's low bit is clear (it always is on x86-64)
                result.push_back(indent + "# opaque predicate: (rsp & 1) == 0");
                result.push_back(indent + "push r11");
                result.push_back(indent + "mov r11, rsp");
                result.push_back(indent + "test r11, 1");  // test bit 0
                // Jump BEFORE pop
                result.push_back(indent + "jnz " + trap_label);  // if bit 0 set (never on aligned stack), trap
                result.push_back(indent + "pop r11");
                result.push_back(indent + "jmp " + skip_label);
                result.push_back(trap_label + ":");
                result.push_back(indent + "pop r11");  // still need to clean stack
                result.push_back(indent + ".byte 0x0f, 0x0b");
                result.push_back(skip_label + ":");
                break;
            }
            case 3:
            default: {
                // Simple: 0 == 0 (xor always sets ZF)
                result.push_back(indent + "# opaque predicate: 0 == 0");
                result.push_back(indent + "push r11");
                result.push_back(indent + "xor r11, r11");
                result.push_back(indent + "test r11, r11");
                // Jump BEFORE pop - pop affects flags!
                result.push_back(indent + "jnz " + trap_label);  // if not zero (never happens), trap
                result.push_back(indent + "pop r11");
                result.push_back(indent + "jmp " + skip_label);
                result.push_back(trap_label + ":");
                result.push_back(indent + "pop r11");  // still need to clean stack
                result.push_back(indent + ".byte 0xcc");
                result.push_back(skip_label + ":");
                break;
            }
        }

        // Restore flags AFTER all operations
        result.push_back(indent + "popfq");

        // Restore red zone protection
        result.push_back(indent + "lea rsp, [rsp + 128]");
    }

    void insertBogusBranch(const std::string& indent, std::vector<std::string>& result) {
        // Insert a conditional jump that's never taken, with junk code after
        // CRITICAL: Use pushfq/popfq to preserve flags!
        // CRITICAL: Protect red zone - functions may use stack below RSP!
        std::string junk_label = "." + generateRandomLabel();
        std::string continue_label = "." + generateRandomLabel();

        result.push_back(indent + "# bogus branch");
        // Protect red zone (128 bytes below RSP on System V AMD64 ABI)
        result.push_back(indent + "lea rsp, [rsp - 128]");
        result.push_back(indent + "pushfq");  // Save flags
        result.push_back(indent + "push r11");
        result.push_back(indent + "xor r11, r11");
        result.push_back(indent + "inc r11");
        result.push_back(indent + "test r11, r11");
        result.push_back(indent + "pop r11");
        result.push_back(indent + "jz " + junk_label);
        result.push_back(indent + "jmp " + continue_label);
        result.push_back(junk_label + ":");

        // Insert confusing junk code (never executed)
        result.push_back(indent + "xor rax, rax");
        result.push_back(indent + "mov rbx, 0xDEADBEEF");
        result.push_back(indent + "call rax");
        result.push_back(indent + ".byte 0x" + toHex(GlobalRandom::nextInt(0, 255)));

        result.push_back(continue_label + ":");
        result.push_back(indent + "popfq");  // Restore flags
        // Restore red zone protection
        result.push_back(indent + "lea rsp, [rsp + 128]");
    }

    void insertPrologueObfuscation(const std::string& indent, std::vector<std::string>& result) {
        // Add some confusing but harmless code after prologue
        // SAFE: Only uses NOPs or flag-preserving operations
        // NOTE: Must protect red zone for cases that use push/pop!
        result.push_back(indent + "# prologue obfuscation");

        int choice = GlobalRandom::nextInt(0, 3);
        switch (choice) {
            case 0:
                // Multi-byte NOPs (safe - no stack usage)
                result.push_back(indent + "nop");
                result.push_back(indent + ".byte 0x66, 0x90");
                result.push_back(indent + ".byte 0x0f, 0x1f, 0x00");
                break;
            case 1:
                // Push/pop that preserves everything (with red zone protection)
                result.push_back(indent + "lea rsp, [rsp - 128]");
                result.push_back(indent + "push r11");
                result.push_back(indent + "pop r11");
                result.push_back(indent + "lea rsp, [rsp + 128]");
                break;
            case 2:
                // Fake stack adjustment with red zone protection
                result.push_back(indent + "lea rsp, [rsp - 128]");
                result.push_back(indent + "pushfq");
                result.push_back(indent + "sub rsp, 8");
                result.push_back(indent + "add rsp, 8");
                result.push_back(indent + "popfq");
                result.push_back(indent + "lea rsp, [rsp + 128]");
                break;
            case 3:
            default:
                // Just NOPs (safe - no stack usage)
                result.push_back(indent + ".byte 0x0f, 0x1f, 0x44, 0x00, 0x00");
                break;
        }

        stats_.increment("prologue_obfuscation");
    }

    // ==================== Instruction Transformations ====================

    std::vector<std::string> transformInstruction(const std::string& trimmed, const std::string& original,
                                                   bool next_uses_flags, bool in_function) {
        std::string indent = getIndent(original);
        std::vector<std::string> result;

        bool this_sets_flags = setsFlags(trimmed);
        bool restrict_transforms = this_sets_flags && next_uses_flags;

        // Try MBA transformations first (most impactful)
        if (config_.enable_mba && !restrict_transforms && GlobalRandom::decide(config_.mba_prob)) {
            if (tryTransformAddMBA(trimmed, indent, result)) return result;
            if (tryTransformSubMBA(trimmed, indent, result)) return result;
            if (tryTransformXorMBA(trimmed, indent, result)) return result;
        }

        // Constant obfuscation
        if (config_.enable_constant_obfuscation && GlobalRandom::decide(config_.constant_split_prob)) {
            if (tryTransformMovImm(trimmed, indent, result)) return result;
        }

        // Standard transformations
        if (!restrict_transforms && GlobalRandom::decide(0.7)) {
            if (tryTransformXorSelf(trimmed, indent, result)) return result;
        }
        if (!restrict_transforms && GlobalRandom::decide(0.7)) {
            if (tryTransformSubSelf(trimmed, indent, result)) return result;
        }
        if (!restrict_transforms && GlobalRandom::decide(0.8)) {
            if (tryTransformIncDec(trimmed, indent, result)) return result;
        }
        if (!restrict_transforms && GlobalRandom::decide(0.5)) {
            if (tryTransformAdd(trimmed, indent, result)) return result;
        }
        if (!restrict_transforms && GlobalRandom::decide(0.5)) {
            if (tryTransformSub(trimmed, indent, result)) return result;
        }
        if (GlobalRandom::decide(0.4)) {
            if (tryTransformMov(trimmed, indent, result)) return result;
        }
        if (!restrict_transforms && GlobalRandom::decide(0.4)) {
            if (tryTransformCmp(trimmed, indent, result)) return result;
        }

        // Dead code insertion
        if (config_.enable_dead_code && in_function && !next_uses_flags) {
            if (GlobalRandom::decide(config_.dead_code_prob)) {
                insertRealDeadCode(indent, result);
                result.push_back(original);
                stats_.increment("dead_code_inserted");
                return result;
            }

            if (GlobalRandom::decide(config_.junk_nop_prob)) {
                insertJunkNop(indent, result);
                result.push_back(original);
                stats_.increment("junk_nops_inserted");
                return result;
            }
        }

        return {};
    }

    // ==================== MBA Transformations ====================

    bool tryTransformAddMBA(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        // add reg1, reg2 -> (reg1 ^ reg2) + 2*(reg1 & reg2)
        // Only apply to 64-bit register operations to avoid size mismatch
        std::regex add_re(R"(add\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, add_re)) {
            std::string dst = match[1].str();
            std::string src = match[2].str();

            if (!isRegister(dst) || !isRegister(src)) return false;
            // Only apply to 64-bit registers to avoid size mismatch with r10/r11
            if (!is64BitReg(dst) || !is64BitReg(src)) return false;
            if (getParentReg64(dst) == "rsp" || getParentReg64(dst) == "rbp") return false;
            if (getParentReg64(src) == "rsp" || getParentReg64(src) == "rbp") return false;

            // add dst, src -> dst = (dst ^ src) + 2*(dst & src)
            // Need to save original dst value
            // CRITICAL: Save/restore flags to not affect subsequent conditional jumps
            // CRITICAL: Protect red zone - functions may use stack below RSP!
            result.push_back(indent + "# MBA: add -> (x^y) + 2*(x&y)");
            result.push_back(indent + "lea rsp, [rsp - 128]");  // Protect red zone
            result.push_back(indent + "pushfq");
            result.push_back(indent + "push r11");
            result.push_back(indent + "push r10");
            result.push_back(indent + "mov r11, " + dst);  // r11 = original dst
            result.push_back(indent + "mov r10, " + dst);  // r10 = original dst
            result.push_back(indent + "xor " + dst + ", " + src);  // dst = dst ^ src
            result.push_back(indent + "and r10, " + src);  // r10 = original_dst & src
            result.push_back(indent + "shl r10, 1");  // r10 = 2 * (original_dst & src)
            result.push_back(indent + "add " + dst + ", r10");  // dst = (dst ^ src) + 2*(dst & src)
            result.push_back(indent + "pop r10");
            result.push_back(indent + "pop r11");
            result.push_back(indent + "popfq");
            result.push_back(indent + "lea rsp, [rsp + 128]");  // Restore red zone

            stats_.increment("mba_add");
            return true;
        }
        return false;
    }

    bool tryTransformSubMBA(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        // sub reg1, reg2 -> (reg1 ^ reg2) - 2*(~reg1 & reg2)
        // Only apply to 64-bit register operations to avoid size mismatch
        std::regex sub_re(R"(sub\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, sub_re)) {
            std::string dst = match[1].str();
            std::string src = match[2].str();

            if (!isRegister(dst) || !isRegister(src)) return false;
            if (toLower(dst) == toLower(src)) return false;  // sub reg, reg is zeroing, handle elsewhere
            // Only apply to 64-bit registers to avoid size mismatch with r10/r11
            if (!is64BitReg(dst) || !is64BitReg(src)) return false;
            if (getParentReg64(dst) == "rsp" || getParentReg64(dst) == "rbp") return false;
            if (getParentReg64(src) == "rsp" || getParentReg64(src) == "rbp") return false;

            // sub dst, src -> dst = (dst ^ src) - 2*(~dst & src)
            // CRITICAL: Save/restore flags to not affect subsequent conditional jumps
            // CRITICAL: Protect red zone - functions may use stack below RSP!
            result.push_back(indent + "# MBA: sub -> (x^y) - 2*(~x&y)");
            result.push_back(indent + "lea rsp, [rsp - 128]");  // Protect red zone
            result.push_back(indent + "pushfq");
            result.push_back(indent + "push r11");
            result.push_back(indent + "push r10");
            result.push_back(indent + "mov r11, " + dst);  // r11 = original dst
            result.push_back(indent + "mov r10, " + dst);  // r10 = original dst
            result.push_back(indent + "not r10");  // r10 = ~dst
            result.push_back(indent + "and r10, " + src);  // r10 = ~dst & src
            result.push_back(indent + "shl r10, 1");  // r10 = 2*(~dst & src)
            result.push_back(indent + "xor " + dst + ", " + src);  // dst = dst ^ src
            result.push_back(indent + "sub " + dst + ", r10");  // dst = (dst ^ src) - 2*(~dst & src)
            result.push_back(indent + "pop r10");
            result.push_back(indent + "pop r11");
            result.push_back(indent + "popfq");
            result.push_back(indent + "lea rsp, [rsp + 128]");  // Restore red zone

            stats_.increment("mba_sub");
            return true;
        }
        return false;
    }

    bool tryTransformXorMBA(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        // xor reg1, reg2 -> (reg1 | reg2) - (reg1 & reg2)
        // Only apply to 64-bit register operations to avoid size mismatch
        std::regex xor_re(R"(xor\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, xor_re)) {
            std::string dst = match[1].str();
            std::string src = match[2].str();

            if (!isRegister(dst) || !isRegister(src)) return false;
            if (toLower(dst) == toLower(src)) return false;  // xor reg, reg is zeroing
            // Only apply to 64-bit registers to avoid size mismatch with r10/r11
            if (!is64BitReg(dst) || !is64BitReg(src)) return false;
            if (getParentReg64(dst) == "rsp" || getParentReg64(dst) == "rbp") return false;
            if (getParentReg64(src) == "rsp" || getParentReg64(src) == "rbp") return false;

            // xor dst, src -> dst = (dst | src) - (dst & src)
            // CRITICAL: Save/restore flags to not affect subsequent conditional jumps
            // CRITICAL: Protect red zone - functions may use stack below RSP!
            result.push_back(indent + "# MBA: xor -> (x|y) - (x&y)");
            result.push_back(indent + "lea rsp, [rsp - 128]");  // Protect red zone
            result.push_back(indent + "pushfq");
            result.push_back(indent + "push r11");
            result.push_back(indent + "push r10");
            result.push_back(indent + "mov r11, " + dst);  // r11 = original dst
            result.push_back(indent + "mov r10, " + dst);  // r10 = original dst
            result.push_back(indent + "or " + dst + ", " + src);  // dst = dst | src
            result.push_back(indent + "and r10, " + src);  // r10 = original_dst & src
            result.push_back(indent + "sub " + dst + ", r10");  // dst = (dst | src) - (dst & src)
            result.push_back(indent + "pop r10");
            result.push_back(indent + "pop r11");
            result.push_back(indent + "popfq");
            result.push_back(indent + "lea rsp, [rsp + 128]");  // Restore red zone

            stats_.increment("mba_xor");
            return true;
        }
        return false;
    }

    // ==================== Standard Transformations ====================

    bool tryTransformXorSelf(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex xor_self(R"(xor\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, xor_self)) {
            std::string reg1 = match[1].str();
            std::string reg2 = match[2].str();

            if (toLower(reg1) == toLower(reg2)) {
                int choice = GlobalRandom::nextInt(0, 2);
                if (choice == 0) {
                    result.push_back(indent + "sub " + reg1 + ", " + reg2);
                } else if (choice == 1) {
                    result.push_back(indent + "mov " + reg1 + ", 0");
                } else {
                    // More complex: and reg, 0
                    result.push_back(indent + "and " + reg1 + ", 0");
                }
                stats_.increment("xor_self_transformed");
                return true;
            }
        }
        return false;
    }

    bool tryTransformSubSelf(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex sub_self(R"(sub\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, sub_self)) {
            std::string reg1 = match[1].str();
            std::string reg2 = match[2].str();

            if (toLower(reg1) == toLower(reg2)) {
                int choice = GlobalRandom::nextInt(0, 1);
                if (choice == 0) {
                    result.push_back(indent + "xor " + reg1 + ", " + reg2);
                } else {
                    result.push_back(indent + "and " + reg1 + ", 0");
                }
                stats_.increment("sub_self_transformed");
                return true;
            }
        }
        return false;
    }

    bool tryTransformMovImm(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex mov_imm(R"(mov\s+(\w+)\s*,\s*(0x[0-9a-fA-F]+|-?\d+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, mov_imm)) {
            std::string reg = match[1].str();
            std::string imm_str = match[2].str();

            if (!isRegister(reg)) return false;
            if (getParentReg64(reg) == "rsp" || getParentReg64(reg) == "rbp") return false;

            int64_t imm;
            try {
                if (imm_str.find("0x") == 0 || imm_str.find("0X") == 0) {
                    imm = std::stoll(imm_str, nullptr, 16);
                } else {
                    imm = std::stoll(imm_str);
                }
            } catch (...) {
                return false;
            }

            // Transform even small constants
            if (imm >= 0 && imm <= 0xFFFFFFFF) {
                int choice = GlobalRandom::nextInt(0, 4);
                uint32_t val = static_cast<uint32_t>(imm);

                if (choice == 0 && val > 0xFF) {
                    // Split: mov reg, high; or reg, low
                    uint32_t high = val & 0xFFFF0000;
                    uint32_t low = val & 0x0000FFFF;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(high));
                    result.push_back(indent + "or " + reg + ", 0x" + toHex(low));
                } else if (choice == 1) {
                    // XOR-based
                    uint32_t val1 = GlobalRandom::nextInt(1, 0xFFFFFF);
                    uint32_t val2 = val ^ val1;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(val1));
                    result.push_back(indent + "xor " + reg + ", 0x" + toHex(val2));
                } else if (choice == 2 && val > 0) {
                    // Add-based
                    uint32_t val1 = GlobalRandom::nextInt(1, val);
                    uint32_t val2 = val - val1;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(val1));
                    result.push_back(indent + "add " + reg + ", 0x" + toHex(val2));
                } else if (choice == 3 && val < 0xFFFFFFF0) {
                    // Sub-based
                    uint32_t val1 = val + GlobalRandom::nextInt(1, 0xFF);
                    uint32_t val2 = val1 - val;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(val1));
                    result.push_back(indent + "sub " + reg + ", 0x" + toHex(val2));
                } else {
                    // NOT-based: mov ~val, then NOT
                    uint32_t notval = ~val;
                    result.push_back(indent + "mov " + reg + ", 0x" + toHex(notval));
                    result.push_back(indent + "not " + reg);
                }

                stats_.increment("constant_obfuscated");
                return true;
            }
        }
        return false;
    }

    bool tryTransformIncDec(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex inc_re(R"(inc\s+(\w+))", std::regex::icase);
        std::regex dec_re(R"(dec\s+(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, inc_re)) {
            std::string reg = match[1].str();
            int choice = GlobalRandom::nextInt(0, 2);
            if (choice == 0) {
                result.push_back(indent + "add " + reg + ", 1");
            } else if (choice == 1) {
                result.push_back(indent + "sub " + reg + ", -1");
            } else {
                result.push_back(indent + "lea " + reg + ", [" + reg + " + 1]");
            }
            stats_.increment("inc_transformed");
            return true;
        }

        if (std::regex_match(trimmed, match, dec_re)) {
            std::string reg = match[1].str();
            int choice = GlobalRandom::nextInt(0, 2);
            if (choice == 0) {
                result.push_back(indent + "sub " + reg + ", 1");
            } else if (choice == 1) {
                result.push_back(indent + "add " + reg + ", -1");
            } else {
                result.push_back(indent + "lea " + reg + ", [" + reg + " - 1]");
            }
            stats_.increment("dec_transformed");
            return true;
        }

        return false;
    }

    bool tryTransformAdd(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex add_reg_imm(R"(add\s+(\w+)\s*,\s*(-?\d+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, add_reg_imm)) {
            std::string reg = match[1].str();
            int imm = std::stoi(match[2].str());

            if (imm > 0 && imm < 128 && detected_arch_ == "x86_64" && isRegister(reg)) {
                std::string parent = getParentReg64(reg);
                if (parent != "rsp" && parent != "rbp") {
                    result.push_back(indent + "lea " + reg + ", [" + reg + " + " + std::to_string(imm) + "]");
                    stats_.increment("add_to_lea");
                    return true;
                }
            }
        }
        return false;
    }

    bool tryTransformSub(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex sub_reg_imm(R"(sub\s+(\w+)\s*,\s*(\d+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, sub_reg_imm)) {
            std::string reg = match[1].str();
            int imm = std::stoi(match[2].str());

            if (imm > 0 && imm < 128) {
                result.push_back(indent + "add " + reg + ", " + std::to_string(-imm));
                stats_.increment("sub_to_add_neg");
                return true;
            }
        }
        return false;
    }

    bool tryTransformMov(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex mov_reg_reg(R"(mov\s+(\w+)\s*,\s*(\w+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, mov_reg_reg)) {
            std::string dst = match[1].str();
            std::string src = match[2].str();

            if (!isRegister(dst) || !isRegister(src)) return false;
            if (toLower(dst) == toLower(src)) return false;

            std::string srcParent = getParentReg64(src);
            std::string dstParent = getParentReg64(dst);
            if (srcParent == "rsp" || dstParent == "rsp") return false;
            if (srcParent == "rbp" || dstParent == "rbp") return false;

            result.push_back(indent + "lea " + dst + ", [" + src + "]");
            stats_.increment("mov_to_lea");
            return true;
        }
        return false;
    }

    bool tryTransformCmp(const std::string& trimmed, const std::string& indent, std::vector<std::string>& result) {
        std::regex cmp_reg_imm(R"(cmp\s+(\w+)\s*,\s*(\d+))", std::regex::icase);
        std::smatch match;

        if (std::regex_match(trimmed, match, cmp_reg_imm)) {
            std::string reg = match[1].str();
            int64_t imm = std::stoll(match[2].str());

            if (!isRegister(reg)) return false;

            if (imm == 0) {
                result.push_back(indent + "test " + reg + ", " + reg);
                stats_.increment("cmp_to_test");
                return true;
            }
        }
        return false;
    }

    // ==================== Dead Code Insertion ====================

    void insertRealDeadCode(const std::string& indent, std::vector<std::string>& result) {
        // CRITICAL: Always preserve flags with pushfq/popfq
        // CRITICAL: Protect red zone - functions may use stack below RSP!
        int choice = GlobalRandom::nextInt(0, 5);

        result.push_back(indent + "lea rsp, [rsp - 128]");  // Protect red zone
        result.push_back(indent + "pushfq");  // Save flags FIRST

        switch (choice) {
            case 0: {
                // Compute something and discard
                result.push_back(indent + "# dead computation");
                result.push_back(indent + "push r11");
                result.push_back(indent + "push r10");
                uint32_t a = GlobalRandom::nextInt(1, 1000);
                uint32_t b = GlobalRandom::nextInt(1, 1000);
                result.push_back(indent + "mov r11, " + std::to_string(a));
                result.push_back(indent + "mov r10, " + std::to_string(b));
                result.push_back(indent + "imul r11, r10");
                result.push_back(indent + "xor r11, r10");
                result.push_back(indent + "pop r10");
                result.push_back(indent + "pop r11");
                break;
            }
            case 1: {
                // Bit manipulation (safe)
                result.push_back(indent + "# dead bit manipulation");
                result.push_back(indent + "push r11");
                result.push_back(indent + "mov r11, 0x" + toHex(GlobalRandom::nextInt(0, 0xFFFFFF)));
                result.push_back(indent + "rol r11, " + std::to_string(GlobalRandom::nextInt(1, 7)));
                result.push_back(indent + "ror r11, " + std::to_string(GlobalRandom::nextInt(1, 7)));
                result.push_back(indent + "bswap r11");
                result.push_back(indent + "pop r11");
                break;
            }
            case 2: {
                // Memory-like operation (stack)
                result.push_back(indent + "# dead stack ops");
                result.push_back(indent + "push r11");
                result.push_back(indent + "push r10");
                result.push_back(indent + "mov r11, [rsp]");
                result.push_back(indent + "xor r11, [rsp + 8]");
                result.push_back(indent + "pop r10");
                result.push_back(indent + "pop r11");
                break;
            }
            case 3: {
                // Push/pop sequence with LEA (doesn't affect flags)
                result.push_back(indent + "# dead lea sequence");
                result.push_back(indent + "push r11");
                result.push_back(indent + "lea r11, [rsp + 8]");
                result.push_back(indent + "lea r11, [r11 - 8]");
                result.push_back(indent + "pop r11");
                break;
            }
            case 4: {
                // Simple push/pop exchange
                result.push_back(indent + "# dead register shuffle");
                result.push_back(indent + "push r11");
                result.push_back(indent + "push r10");
                result.push_back(indent + "mov r11, r10");
                result.push_back(indent + "mov r10, r11");
                result.push_back(indent + "pop r10");
                result.push_back(indent + "pop r11");
                break;
            }
            case 5:
            default: {
                // Simple NOPs with register save
                result.push_back(indent + "# dead nop sequence");
                result.push_back(indent + "push r11");
                result.push_back(indent + "xchg r11, r11");  // No-op exchange
                result.push_back(indent + "pop r11");
                break;
            }
        }

        result.push_back(indent + "popfq");  // Restore flags LAST
        result.push_back(indent + "lea rsp, [rsp + 128]");  // Restore red zone
    }

    void insertJunkNop(const std::string& indent, std::vector<std::string>& result) {
        int choice = GlobalRandom::nextInt(0, 6);

        switch (choice) {
            case 0:
            case 1:
                result.push_back(indent + "nop");
                break;
            case 2:
                result.push_back(indent + ".byte 0x66, 0x90");  // 2-byte nop
                break;
            case 3:
                result.push_back(indent + ".byte 0x0f, 0x1f, 0x00");  // 3-byte nop
                break;
            case 4:
                result.push_back(indent + ".byte 0x0f, 0x1f, 0x44, 0x00, 0x00");  // 5-byte nop
                break;
            case 5:
                result.push_back(indent + "fnop");
                break;
            case 6:
            default:
                result.push_back(indent + ".byte 0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00");  // 7-byte nop
                break;
        }
    }

    // ==================== Utility Functions ====================

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
        if (trimmed[0] == '.') return true;
        if (trimmed[0] == '#') return true;
        if (trimmed[0] == ';') return true;
        if (trimmed.find(':') != std::string::npos) return true;
        return false;
    }

    bool setsFlags(const std::string& trimmed) {
        static const std::vector<std::string> flag_setters = {
            "cmp", "test", "add", "sub", "and", "or", "xor", "inc", "dec",
            "neg", "not", "shl", "shr", "sar", "sal", "rol", "ror",
            "adc", "sbb", "imul", "mul", "div", "idiv", "bt", "bts", "btr", "btc"
        };
        std::string lower = toLower(trimmed);
        for (const auto& setter : flag_setters) {
            if (lower.find(setter) == 0 &&
                (lower.size() == setter.size() || !std::isalpha(lower[setter.size()]))) {
                return true;
            }
        }
        return false;
    }

    bool usesFlags(const std::string& trimmed) {
        static const std::vector<std::string> flag_users = {
            "je", "jne", "jz", "jnz", "jg", "jge", "jl", "jle",
            "ja", "jae", "jb", "jbe", "jo", "jno", "js", "jns",
            "jp", "jnp", "jc", "jnc",
            "cmove", "cmovne", "cmovz", "cmovnz", "cmovg", "cmovge",
            "cmovl", "cmovle", "cmova", "cmovae", "cmovb", "cmovbe",
            "sete", "setne", "setz", "setnz", "setg", "setge",
            "setl", "setle", "seta", "setae", "setb", "setbe",
            "adc", "sbb", "rcl", "rcr"
        };
        std::string lower = toLower(trimmed);
        for (const auto& user : flag_users) {
            if (lower.find(user) == 0 &&
                (lower.size() == user.size() || !std::isalpha(lower[user.size()]))) {
                return true;
            }
        }
        return false;
    }

    bool isControlFlow(const std::string& trimmed) {
        static const std::vector<std::string> control = {
            "jmp", "je", "jne", "jz", "jnz", "jg", "jge", "jl", "jle",
            "ja", "jae", "jb", "jbe", "jo", "jno", "js", "jns",
            "jp", "jnp", "jc", "jnc", "call", "ret", "leave",
            "loop", "loope", "loopne", "loopz", "loopnz", "syscall", "int"
        };
        std::string lower = toLower(trimmed);
        for (const auto& cf : control) {
            if (lower.find(cf) == 0 &&
                (lower.size() == cf.size() || !std::isalpha(lower[cf.size()]))) {
                return true;
            }
        }
        return false;
    }

    bool checkNextUsesFlags(const std::vector<std::string>& lines, size_t i) {
        for (size_t j = i + 1; j < lines.size() && j < i + 5; j++) {
            std::string next = trim(lines[j]);
            if (!shouldSkipLine(next)) {
                return usesFlags(next);
            }
        }
        return false;
    }

    std::string getParentReg64(const std::string& reg) {
        std::string lower = toLower(reg);
        auto it = reg_to_parent64_.find(lower);
        return it != reg_to_parent64_.end() ? it->second : "";
    }

    bool isRegister(const std::string& operand) {
        return all_registers_.count(toLower(operand)) > 0;
    }

    bool is64BitReg(const std::string& reg) {
        std::string lower = toLower(reg);
        // r8-r15 are 64-bit
        if (lower[0] == 'r' && lower.size() >= 2 && std::isdigit(lower[1])) {
            // Check it's not r8d, r8w, r8b etc
            return lower.size() == 2 || (lower.size() == 3 && std::isdigit(lower[2]));
        }
        // rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp
        static std::set<std::string> regs64 = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp"};
        return regs64.count(lower) > 0;
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
            config_.global_probability = json["global_probability"].asDouble(0.7);
        }

        const JsonValue& settings = json.has("obfuscation_settings")
            ? json["obfuscation_settings"] : json;

        if (settings.has("global_probability")) {
            config_.global_probability = settings["global_probability"].asDouble(0.7);
        }

        if (json.has("assembly")) {
            const auto& asm_settings = json["assembly"];
            if (asm_settings.has("transforms_enabled")) {
                config_.transforms_enabled = asm_settings["transforms_enabled"].asBool(true);
            }
            if (asm_settings.has("control_flow")) {
                config_.enable_control_flow = asm_settings["control_flow"].asBool(true);
            }
            if (asm_settings.has("mba")) {
                config_.enable_mba = asm_settings["mba"].asBool(true);
            }
            if (asm_settings.has("string_encryption")) {
                config_.enable_string_encryption = asm_settings["string_encryption"].asBool(true);
            }
        }

        logger_.info("Loaded configuration (transforms: {})", config_.transforms_enabled ? "enabled" : "disabled");
    }
};

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
    std::cout << "Features:" << std::endl;
    std::cout << "  - Control flow obfuscation (opaque predicates, bogus branches)" << std::endl;
    std::cout << "  - MBA (Mixed Boolean-Arithmetic) transformations" << std::endl;
    std::cout << "  - Constant obfuscation" << std::endl;
    std::cout << "  - String encryption (XOR)" << std::endl;
    std::cout << "  - Dead code insertion" << std::endl;
    std::cout << "  - Label randomization" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file;
    std::string input_file;
    std::string output_file;
    double probability = -1;
    bool verbose = false;

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

    if (verbose) {
        LogConfig::get().setLevel(LogLevel::Debug);
    }

    printBanner();

    AssemblyObfuscator obfuscator;

    if (!config_file.empty()) {
        if (!obfuscator.loadConfig(config_file)) {
            return 1;
        }
    }

    if (probability >= 0) {
        obfuscator.setGlobalProbability(probability);
    }

    std::ifstream input(input_file);
    if (!input.is_open()) {
        LOG_ERROR("Cannot open input file: {}", input_file);
        return 1;
    }

    std::string asm_code((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
    input.close();

    LOG_INFO("Read {} bytes from {}", asm_code.size(), input_file);

    std::string obfuscated = obfuscator.obfuscate(asm_code);

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
