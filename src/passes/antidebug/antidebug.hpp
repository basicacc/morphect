/**
 * Morphect - Anti-Debugging Transformations
 *
 * Implements anti-debugging techniques:
 *   - ptrace detection (PTRACE_TRACEME)
 *   - Timing-based detection (RDTSC, clock_gettime)
 *   - Environment variable checks
 *   - TracerPid check (/proc/self/status)
 *   - Parent process checks
 *
 * These can be applied at:
 *   - Source code level (C code generation)
 *   - LLVM IR level (IR snippets)
 *   - Assembly level (inline asm)
 */

#ifndef MORPHECT_ANTIDEBUG_HPP
#define MORPHECT_ANTIDEBUG_HPP

#include "antidebug_base.hpp"

#include <regex>
#include <sstream>
#include <algorithm>

namespace morphect {
namespace antidebug {

/**
 * LLVM IR Anti-Debugging Transformation
 *
 * Inserts anti-debugging checks into LLVM IR
 */
class LLVMAntiDebugTransformation {
public:
    std::string getName() const { return "LLVM_AntiDebug"; }

    AntiDebugResult transform(
        const std::vector<std::string>& lines,
        const AntiDebugConfig& config) {

        AntiDebugResult result;
        result.generated_code = lines;

        if (!config.enabled) {
            result.success = true;
            return result;
        }

        // Find insertion points
        AntiDebugAnalyzer analyzer;
        auto entry_points = analyzer.findFunctionEntries(lines);
        auto loop_headers = config.insert_at_loops ?
            analyzer.findLoopHeaders(lines) : std::vector<int>();

        // Track insertions
        std::vector<std::pair<int, std::vector<std::string>>> insertions;

        // Insert at function entries
        if (config.insert_at_entry) {
            int checks_inserted = 0;
            for (int point : entry_points) {
                if (checks_inserted >= config.max_checks_per_function) break;
                if (GlobalRandom::nextDouble() > config.probability) continue;

                std::vector<std::string> check_code;
                std::string fail_label = "_antidebug_fail_" +
                    std::to_string(label_counter_++);

                // Generate timing check
                if (config.use_timing) {
                    auto timing = LLVMAntiDebugGenerator::generateTimingCheck(
                        config.response,
                        config.timing_threshold_ns,
                        fail_label);
                    check_code.insert(check_code.end(), timing.begin(), timing.end());
                    result.techniques_used.push_back(AntiDebugTechnique::TimingCheck);
                }

                // Generate env checks
                if (config.use_env_check) {
                    auto env = LLVMAntiDebugGenerator::generateEnvCheck(
                        "LD_PRELOAD", fail_label);
                    check_code.insert(check_code.end(), env.begin(), env.end());
                    result.techniques_used.push_back(AntiDebugTechnique::EnvironmentCheck);
                }

                if (!check_code.empty()) {
                    insertions.push_back({point, check_code});
                    result.checks_inserted++;
                    checks_inserted++;
                }
            }
        }

        // Insert at loop headers (less frequently)
        if (config.insert_at_loops) {
            for (int point : loop_headers) {
                if (GlobalRandom::nextDouble() > config.probability * 0.5) continue;

                std::string fail_label = "_antidebug_loop_fail_" +
                    std::to_string(label_counter_++);

                auto timing = LLVMAntiDebugGenerator::generateTimingCheck(
                    config.response,
                    config.timing_threshold_ns * 2,  // More lenient in loops
                    fail_label);

                insertions.push_back({point, timing});
                result.checks_inserted++;
            }
        }

        // Sort insertions by position (descending) for safe insertion
        std::sort(insertions.begin(), insertions.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        // Apply insertions
        for (const auto& [pos, code] : insertions) {
            if (pos >= 0 && pos <= static_cast<int>(result.generated_code.size())) {
                result.generated_code.insert(
                    result.generated_code.begin() + pos,
                    code.begin(), code.end());
            }
        }

        result.success = true;
        return result;
    }

private:
    static int label_counter_;
};

inline int LLVMAntiDebugTransformation::label_counter_ = 0;

/**
 * Assembly Anti-Debugging Transformation
 *
 * Inserts anti-debugging checks into x86/x64 assembly
 */
class AssemblyAntiDebugTransformation {
public:
    std::string getName() const { return "Asm_AntiDebug"; }

    AntiDebugResult transform(
        const std::vector<std::string>& lines,
        const AntiDebugConfig& config) {

        AntiDebugResult result;
        result.generated_code = lines;

        if (!config.enabled) {
            result.success = true;
            return result;
        }

        // Find insertion points
        AntiDebugAnalyzer analyzer;
        auto entry_points = analyzer.findAsmFunctionEntries(lines);

        // Track insertions
        std::vector<std::pair<int, std::vector<std::string>>> insertions;

        // Generate fail label and handler at the end
        std::string fail_label = ".Lantidebug_fail";
        std::vector<std::string> fail_handler;
        fail_handler.push_back("");
        fail_handler.push_back("    # Anti-debug: detection handler");
        fail_handler.push_back(fail_label + ":");

        switch (config.response) {
            case AntiDebugResponse::Exit:
                fail_handler.push_back("    mov edi, 1");
                fail_handler.push_back("    mov eax, 60       # __NR_exit");
                fail_handler.push_back("    syscall");
                break;

            case AntiDebugResponse::Crash:
                fail_handler.push_back("    xor rax, rax");
                fail_handler.push_back("    mov qword ptr [rax], 0  # Crash");
                break;

            case AntiDebugResponse::InfiniteLoop:
                fail_handler.push_back(".Linf_loop:");
                fail_handler.push_back("    jmp .Linf_loop");
                break;

            default:
                fail_handler.push_back("    mov edi, 1");
                fail_handler.push_back("    mov eax, 60");
                fail_handler.push_back("    syscall");
                break;
        }

        // Insert checks at function entries
        if (config.insert_at_entry) {
            int checks_inserted = 0;
            for (int point : entry_points) {
                if (checks_inserted >= config.max_checks_per_function) break;
                if (GlobalRandom::nextDouble() > config.probability) continue;

                std::vector<std::string> check_code;

                // Save registers we'll use
                check_code.push_back("    # Anti-debug checks");
                check_code.push_back("    push rax");
                check_code.push_back("    push rcx");
                check_code.push_back("    push rdx");
                check_code.push_back("    push r15");

                // Add timing check
                if (config.use_timing) {
                    uint64_t threshold = (config.timing_threshold_ns / 1000) * 3;
                    auto timing = X86AntiDebugGenerator::generateTimingCheckAsm(
                        threshold, fail_label);
                    check_code.insert(check_code.end(), timing.begin(), timing.end());
                    result.techniques_used.push_back(AntiDebugTechnique::TimingCheck);
                }

                // Add ptrace check (syscall based)
                if (config.use_ptrace) {
                    auto ptrace = X86AntiDebugGenerator::generatePtraceSyscallAsm(
                        fail_label);
                    check_code.insert(check_code.end(), ptrace.begin(), ptrace.end());
                    result.techniques_used.push_back(AntiDebugTechnique::PtraceDetection);
                }

                // Restore registers
                check_code.push_back("    pop r15");
                check_code.push_back("    pop rdx");
                check_code.push_back("    pop rcx");
                check_code.push_back("    pop rax");

                if (!check_code.empty()) {
                    insertions.push_back({point, check_code});
                    result.checks_inserted++;
                    checks_inserted++;
                }
            }
        }

        // Find position to add fail handler (after last .size directive)
        int handler_pos = static_cast<int>(lines.size());
        for (int i = static_cast<int>(lines.size()) - 1; i >= 0; i--) {
            if (lines[i].find(".size") != std::string::npos) {
                handler_pos = i + 1;
                break;
            }
        }

        if (result.checks_inserted > 0) {
            insertions.push_back({handler_pos, fail_handler});
        }

        // Sort insertions by position (descending) for safe insertion
        std::sort(insertions.begin(), insertions.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        // Apply insertions
        for (const auto& [pos, code] : insertions) {
            if (pos >= 0 && pos <= static_cast<int>(result.generated_code.size())) {
                result.generated_code.insert(
                    result.generated_code.begin() + pos,
                    code.begin(), code.end());
            }
        }

        result.success = true;
        return result;
    }
};

/**
 * C Source Code Anti-Debugging Generator
 *
 * Generates C code that can be included in source files
 */
class CAntiDebugGenerator {
public:
    /**
     * Generate a complete anti-debug header file
     */
    static std::vector<std::string> generateHeader(
        const AntiDebugConfig& config) {

        std::vector<std::string> code;

        code.push_back("/**");
        code.push_back(" * Morphect Anti-Debugging Header");
        code.push_back(" * Auto-generated - do not edit");
        code.push_back(" */");
        code.push_back("");
        code.push_back("#ifndef MORPHECT_ANTIDEBUG_H");
        code.push_back("#define MORPHECT_ANTIDEBUG_H");
        code.push_back("");
        code.push_back("#ifdef __cplusplus");
        code.push_back("extern \"C\" {");
        code.push_back("#endif");
        code.push_back("");

        // Add the combined check
        auto combined = LinuxAntiDebugGenerator::generateCombinedCheck(config);
        code.insert(code.end(), combined.begin(), combined.end());

        code.push_back("");
        code.push_back("// Call this at program start or in sensitive functions");
        code.push_back("#define MORPHECT_ANTIDEBUG_CHECK() morphect_antidebug_check()");
        code.push_back("");
        code.push_back("#ifdef __cplusplus");
        code.push_back("}");
        code.push_back("#endif");
        code.push_back("");
        code.push_back("#endif // MORPHECT_ANTIDEBUG_H");

        return code;
    }

    /**
     * Generate individual technique as standalone code
     */
    static std::vector<std::string> generateTechnique(
        AntiDebugTechnique technique,
        const AntiDebugConfig& config) {

        switch (technique) {
            case AntiDebugTechnique::PtraceDetection:
                return LinuxAntiDebugGenerator::generatePtraceCheck(
                    config.response, config.obfuscate_checks);

            case AntiDebugTechnique::TimingCheck:
                return LinuxAntiDebugGenerator::generateTimingCheck(
                    config.response, config.timing_threshold_ns,
                    true, config.obfuscate_checks);

            case AntiDebugTechnique::EnvironmentCheck:
                return LinuxAntiDebugGenerator::generateEnvCheck(
                    config.response, config.obfuscate_checks);

            case AntiDebugTechnique::DebugFlagsCheck:
                return LinuxAntiDebugGenerator::generateTracerPidCheck(
                    config.response, config.obfuscate_checks);

            case AntiDebugTechnique::ParentProcessCheck:
                return LinuxAntiDebugGenerator::generateParentCheck(
                    config.response, config.obfuscate_checks);

            default:
                return {"// Technique not implemented"};
        }
    }
};

/**
 * Assembly Anti-Debugging Pass
 */
class AssemblyAntiDebugPass : public AssemblyTransformationPass {
public:
    AssemblyAntiDebugPass() : transformer_() {}

    std::string getName() const override { return "AntiDebug"; }
    std::string getDescription() const override {
        return "Inserts anti-debugging checks into assembly";
    }

    PassPriority getPriority() const override { return PassPriority::Late; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        ad_config_.enabled = config.enabled;
        ad_config_.probability = config.probability;
        return true;
    }

    void setAntiDebugConfig(const AntiDebugConfig& config) {
        ad_config_ = config;
    }

    const AntiDebugConfig& getAntiDebugConfig() const {
        return ad_config_;
    }

    TransformResult transformAssembly(
        std::vector<std::string>& lines,
        const std::string& arch) override {

        if (!ad_config_.enabled) {
            return TransformResult::Skipped;
        }

        // Only support Linux for now
        if (ad_config_.target_os != TargetOS::Linux) {
            return TransformResult::Skipped;
        }

        auto result = transformer_.transform(lines, ad_config_);

        if (result.success) {
            lines = std::move(result.generated_code);
            statistics_["checks_inserted"] = result.checks_inserted;
            statistics_["techniques_used"] = static_cast<int>(
                result.techniques_used.size());
            return TransformResult::Success;
        } else {
            return TransformResult::Error;
        }
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    AssemblyAntiDebugTransformation transformer_;
    AntiDebugConfig ad_config_;
};

/**
 * LLVM IR Anti-Debugging Pass
 */
class LLVMAntiDebugPass : public LLVMTransformationPass {
public:
    LLVMAntiDebugPass() : transformer_() {}

    std::string getName() const override { return "AntiDebug"; }
    std::string getDescription() const override {
        return "Inserts anti-debugging checks into LLVM IR";
    }

    PassPriority getPriority() const override { return PassPriority::Late; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        ad_config_.enabled = config.enabled;
        ad_config_.probability = config.probability;
        return true;
    }

    void setAntiDebugConfig(const AntiDebugConfig& config) {
        ad_config_ = config;
    }

    void setTargetTriple(const std::string& triple) {
        // Set target OS from triple
        if (triple.find("linux") != std::string::npos) {
            ad_config_.target_os = TargetOS::Linux;
        } else if (triple.find("darwin") != std::string::npos ||
                   triple.find("macos") != std::string::npos) {
            ad_config_.target_os = TargetOS::MacOS;
        } else if (triple.find("windows") != std::string::npos ||
                   triple.find("win32") != std::string::npos) {
            ad_config_.target_os = TargetOS::Windows;
        }
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!ad_config_.enabled) {
            return TransformResult::Skipped;
        }

        auto result = transformer_.transform(lines, ad_config_);

        if (result.success) {
            lines = std::move(result.generated_code);
            statistics_["checks_inserted"] = result.checks_inserted;
            return TransformResult::Success;
        } else {
            return TransformResult::Error;
        }
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    LLVMAntiDebugTransformation transformer_;
    AntiDebugConfig ad_config_;
};

} // namespace antidebug
} // namespace morphect

#endif // MORPHECT_ANTIDEBUG_HPP
