/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * antidebug_base.hpp - Base definitions for anti-debugging techniques
 *
 * Anti-debugging techniques detect or thwart debugger attachment:
 *   - ptrace detection (Linux)
 *   - Timing-based detection (measure execution time)
 *   - Environment variable checks
 *   - Parent process checks
 *   - Debug register detection
 *   - Breakpoint detection
 *
 * WARNING: Anti-debugging can break legitimate debugging and crash handlers.
 * Use with caution and always provide a way to disable it.
 */

#ifndef MORPHECT_ANTIDEBUG_BASE_HPP
#define MORPHECT_ANTIDEBUG_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace morphect {
namespace antidebug {

/**
 * Types of anti-debugging techniques
 */
enum class AntiDebugTechnique {
    PtraceDetection,        // Linux ptrace TRACEME check
    TimingCheck,            // RDTSC/clock timing checks
    EnvironmentCheck,       // Check for debugger env vars
    ParentProcessCheck,     // Check parent process name
    DebugFlagsCheck,        // Check /proc/self/status TracerPid
    BreakpointDetection,    // Check for software breakpoints
    HardwareBreakpoints,    // Check debug registers
    IsDebuggerPresent,      // Windows API (for cross-platform reference)
    ExceptionHandling,      // SEH/signal-based detection
    SelfDebugging,          // Attach to self to prevent others
};

/**
 * Response when debugger is detected
 */
enum class AntiDebugResponse {
    Exit,                   // Exit immediately
    Crash,                  // Cause a crash (dereference null, etc.)
    InfiniteLoop,           // Hang the program
    CorruptData,            // Corrupt internal data/keys
    ReturnWrong,            // Return wrong results
    Silent,                 // Continue but set internal flag
    Custom,                 // Call user-provided callback
};

/**
 * Target operating system
 */
enum class TargetOS {
    Linux,
    Windows,
    MacOS,
    BSD,
    Generic     // Cross-platform checks only
};

/**
 * Configuration for anti-debugging
 */
struct AntiDebugConfig {
    bool enabled = true;
    TargetOS target_os = TargetOS::Linux;
    AntiDebugResponse response = AntiDebugResponse::Exit;

    // Which techniques to use
    bool use_ptrace = true;              // Linux ptrace check
    bool use_timing = true;              // Timing-based detection
    bool use_env_check = true;           // Environment variable check
    bool use_parent_check = true;        // Parent process check
    bool use_tracer_pid = true;          // /proc/self/status TracerPid
    bool use_breakpoint_check = false;   // Check for int3 bytes
    bool use_hardware_bp_check = false;  // Check debug registers (needs root)
    bool use_exception_check = false;    // Signal/SEH-based

    // Timing settings
    uint64_t timing_threshold_ns = 100000000;  // 100ms - suspiciously slow
    int timing_samples = 3;                     // Number of timing samples

    // Response settings
    std::string custom_response_code;    // For Custom response
    bool obfuscate_checks = true;        // Use MBA/opaque predicates in checks

    // Safety
    bool allow_disable_via_env = false;  // Allow MORPHECT_NODEBUGCHECK=1
    std::string disable_env_var = "MORPHECT_NODEBUGCHECK";

    // Insertion settings
    double probability = 0.3;            // Probability at each insertion point
    int max_checks_per_function = 2;     // Max anti-debug checks per function
    bool insert_at_entry = true;         // Insert check at function entry
    bool insert_at_loops = false;        // Insert check in loops
    bool insert_random = true;           // Insert at random points
};

/**
 * Result of anti-debugging code generation
 */
struct AntiDebugResult {
    bool success = false;
    std::vector<std::string> generated_code;
    int checks_inserted = 0;
    std::vector<AntiDebugTechnique> techniques_used;
    std::string error_message;
};

// ============================================================================
// Linux Anti-Debugging Code Generators
// ============================================================================

/**
 * Generates C code snippets for anti-debugging
 * These are meant to be inserted into source code or generated at compile time
 */
class LinuxAntiDebugGenerator {
public:
    /**
     * Generate ptrace detection code (C)
     *
     * The ptrace(PTRACE_TRACEME) trick: if we're already being traced,
     * this call will fail. If not, we trace ourselves and must detach.
     */
    static std::vector<std::string> generatePtraceCheck(
        AntiDebugResponse response,
        bool obfuscate = true) {

        std::vector<std::string> code;

        code.push_back("// Anti-debug: ptrace detection");
        code.push_back("#include <sys/ptrace.h>");
        code.push_back("#include <stdlib.h>");
        code.push_back("");
        code.push_back("static inline void __attribute__((always_inline)) _check_ptrace(void) {");

        if (obfuscate) {
            // Obfuscated version using indirect call
            code.push_back("    long (*_pt)(int, ...) = (long(*)(int, ...))ptrace;");
            code.push_back("    volatile long _r = _pt(0, 0, 0, 0);  // PTRACE_TRACEME = 0");
            code.push_back("    if (_r == -1) {");
        } else {
            code.push_back("    if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {");
        }

        // Response based on configuration
        appendResponse(code, response, "        ");

        code.push_back("    }");

        if (obfuscate) {
            // Detach from self if we succeeded
            code.push_back("    _pt(7, 0, 0, 0);  // PTRACE_DETACH = 7");
        } else {
            code.push_back("    ptrace(PTRACE_DETACH, 0, 0, 0);");
        }

        code.push_back("}");

        return code;
    }

    /**
     * Generate timing-based detection code (C)
     *
     * Uses RDTSC or clock_gettime to measure code execution time.
     * Debugger stepping will cause abnormally long execution times.
     */
    static std::vector<std::string> generateTimingCheck(
        AntiDebugResponse response,
        uint64_t threshold_ns = 100000000,
        bool use_rdtsc = true,
        bool obfuscate = true) {

        std::vector<std::string> code;

        code.push_back("// Anti-debug: timing detection");
        code.push_back("#include <stdint.h>");
        code.push_back("#include <time.h>");
        code.push_back("");

        if (use_rdtsc) {
            // RDTSC version (faster, harder to fake)
            code.push_back("static inline uint64_t __attribute__((always_inline)) _rdtsc(void) {");
            code.push_back("    uint32_t lo, hi;");
            code.push_back("    __asm__ volatile (\"rdtsc\" : \"=a\"(lo), \"=d\"(hi));");
            code.push_back("    return ((uint64_t)hi << 32) | lo;");
            code.push_back("}");
            code.push_back("");
            code.push_back("static inline void __attribute__((always_inline)) _timing_check(void) {");
            code.push_back("    volatile uint64_t _t1 = _rdtsc();");

            // Do some trivial work
            code.push_back("    volatile int _dummy = 0;");
            code.push_back("    for (int i = 0; i < 100; i++) _dummy += i;");

            code.push_back("    volatile uint64_t _t2 = _rdtsc();");
            code.push_back("    uint64_t _diff = _t2 - _t1;");

            // Threshold in cycles (rough: 100ms at 3GHz = 300M cycles)
            uint64_t cycle_threshold = (threshold_ns / 1000) * 3;  // Rough estimate
            code.push_back("    if (_diff > " + std::to_string(cycle_threshold) + "ULL) {");
        } else {
            // clock_gettime version (more portable)
            code.push_back("static inline void __attribute__((always_inline)) _timing_check(void) {");
            code.push_back("    struct timespec _ts1, _ts2;");
            code.push_back("    clock_gettime(CLOCK_MONOTONIC, &_ts1);");

            // Do some trivial work
            code.push_back("    volatile int _dummy = 0;");
            code.push_back("    for (int i = 0; i < 100; i++) _dummy += i;");

            code.push_back("    clock_gettime(CLOCK_MONOTONIC, &_ts2);");
            code.push_back("    uint64_t _diff = (_ts2.tv_sec - _ts1.tv_sec) * 1000000000ULL + ");
            code.push_back("                     (_ts2.tv_nsec - _ts1.tv_nsec);");
            code.push_back("    if (_diff > " + std::to_string(threshold_ns) + "ULL) {");
        }

        appendResponse(code, response, "        ");

        code.push_back("    }");
        code.push_back("}");

        return code;
    }

    /**
     * Generate environment variable check code (C)
     *
     * Checks for debugger-related environment variables
     */
    static std::vector<std::string> generateEnvCheck(
        AntiDebugResponse response,
        bool obfuscate = true) {

        std::vector<std::string> code;

        code.push_back("// Anti-debug: environment check");
        code.push_back("#include <stdlib.h>");
        code.push_back("#include <string.h>");
        code.push_back("");
        code.push_back("static inline void __attribute__((always_inline)) _env_check(void) {");

        // List of suspicious environment variables
        std::vector<std::string> env_vars = {
            "LD_PRELOAD",           // Library injection
            "LD_DEBUG",             // Linker debugging
            "MALLOC_CHECK_",        // Memory debugging
            "MALLOC_TRACE",         // Memory tracing
            "_",                    // Often contains debugger path
        };

        for (const auto& var : env_vars) {
            if (obfuscate) {
                // Encode variable name
                code.push_back("    {");
                code.push_back("        char _v[] = \"" + encodeString(var) + "\";");
                code.push_back("        _decode_str(_v, " + std::to_string(var.length()) + ");");
                code.push_back("        char* _e = getenv(_v);");
            } else {
                code.push_back("    {");
                code.push_back("        char* _e = getenv(\"" + var + "\");");
            }

            if (var == "_") {
                // Check if _ contains debugger names
                code.push_back("        if (_e && (strstr(_e, \"gdb\") || strstr(_e, \"lldb\") ||");
                code.push_back("                   strstr(_e, \"strace\") || strstr(_e, \"ltrace\"))) {");
            } else {
                code.push_back("        if (_e) {");
            }

            appendResponse(code, response, "            ");

            code.push_back("        }");
            code.push_back("    }");
        }

        code.push_back("}");

        // Add decode helper if obfuscating
        if (obfuscate) {
            code.insert(code.begin() + 4, "static inline void _decode_str(char* s, int len) {");
            code.insert(code.begin() + 5, "    for (int i = 0; i < len; i++) s[i] ^= 0x42;");
            code.insert(code.begin() + 6, "}");
            code.insert(code.begin() + 7, "");
        }

        return code;
    }

    /**
     * Generate /proc/self/status TracerPid check (C)
     *
     * Reads /proc/self/status and checks if TracerPid is non-zero
     */
    static std::vector<std::string> generateTracerPidCheck(
        AntiDebugResponse response,
        bool obfuscate = true) {

        std::vector<std::string> code;

        code.push_back("// Anti-debug: TracerPid check");
        code.push_back("#include <stdio.h>");
        code.push_back("#include <string.h>");
        code.push_back("#include <stdlib.h>");
        code.push_back("");
        code.push_back("static inline void __attribute__((always_inline)) _tracer_check(void) {");

        if (obfuscate) {
            // Obfuscated path
            code.push_back("    char _p[] = \"\\x32\\x70\\x72\\x6f\\x63\\x2f\\x73\\x65\\x6c\\x66\\x2f\\x73\\x74\\x61\\x74\\x75\\x73\";");
            code.push_back("    _p[0] = '/';  // Fix first char");
        } else {
            code.push_back("    const char* _p = \"/proc/self/status\";");
        }

        code.push_back("    FILE* _f = fopen(_p, \"r\");");
        code.push_back("    if (_f) {");
        code.push_back("        char _line[256];");
        code.push_back("        while (fgets(_line, sizeof(_line), _f)) {");
        code.push_back("            if (strncmp(_line, \"TracerPid:\", 10) == 0) {");
        code.push_back("                int _pid = atoi(_line + 10);");
        code.push_back("                fclose(_f);");
        code.push_back("                if (_pid != 0) {");

        appendResponse(code, response, "                    ");

        code.push_back("                }");
        code.push_back("                return;");
        code.push_back("            }");
        code.push_back("        }");
        code.push_back("        fclose(_f);");
        code.push_back("    }");
        code.push_back("}");

        return code;
    }

    /**
     * Generate parent process check (C)
     *
     * Checks if parent process is a known debugger
     */
    static std::vector<std::string> generateParentCheck(
        AntiDebugResponse response,
        bool obfuscate = true) {

        std::vector<std::string> code;

        code.push_back("// Anti-debug: parent process check");
        code.push_back("#include <stdio.h>");
        code.push_back("#include <string.h>");
        code.push_back("#include <unistd.h>");
        code.push_back("");
        code.push_back("static inline void __attribute__((always_inline)) _parent_check(void) {");
        code.push_back("    char _path[64];");
        code.push_back("    char _name[256];");
        code.push_back("    snprintf(_path, sizeof(_path), \"/proc/%d/comm\", getppid());");
        code.push_back("    FILE* _f = fopen(_path, \"r\");");
        code.push_back("    if (_f) {");
        code.push_back("        if (fgets(_name, sizeof(_name), _f)) {");

        // Check for known debuggers
        std::vector<std::string> debuggers = {"gdb", "lldb", "strace", "ltrace", "radare2", "r2"};

        code.push_back("            if (");
        for (size_t i = 0; i < debuggers.size(); i++) {
            std::string check = "                strstr(_name, \"" + debuggers[i] + "\")";
            if (i < debuggers.size() - 1) {
                check += " ||";
            } else {
                check += ") {";
            }
            code.push_back(check);
        }

        code.push_back("                fclose(_f);");
        appendResponse(code, response, "                ");

        code.push_back("            }");
        code.push_back("        }");
        code.push_back("        fclose(_f);");
        code.push_back("    }");
        code.push_back("}");

        return code;
    }

    /**
     * Generate combined anti-debug check that uses multiple techniques
     */
    static std::vector<std::string> generateCombinedCheck(
        const AntiDebugConfig& config) {

        std::vector<std::string> code;

        code.push_back("// Anti-debug: combined check");
        code.push_back("#include <stdio.h>");
        code.push_back("#include <stdlib.h>");
        code.push_back("#include <string.h>");
        code.push_back("#include <stdint.h>");
        code.push_back("#include <unistd.h>");
        code.push_back("");

        if (config.use_ptrace) {
            code.push_back("#include <sys/ptrace.h>");
        }
        if (config.use_timing) {
            code.push_back("#include <time.h>");
        }
        if (config.use_parent_check) {
            code.push_back("#include <unistd.h>");
        }

        code.push_back("");

        // Add helper functions
        if (config.obfuscate_checks) {
            code.push_back("static inline void _decode_str(char* s, int len) {");
            code.push_back("    for (int i = 0; i < len; i++) s[i] ^= 0x42;");
            code.push_back("}");
            code.push_back("");
        }

        // Response function
        code.push_back("static void __attribute__((noinline)) _dbg_detected(void) {");
        appendResponse(code, config.response, "    ");
        code.push_back("}");
        code.push_back("");

        // Main check function
        code.push_back("static inline void __attribute__((always_inline)) morphect_antidebug_check(void) {");
        code.push_back("    volatile int _detected = 0;");
        code.push_back("");

        if (config.allow_disable_via_env) {
            code.push_back("    // Allow disable via environment");
            code.push_back("    if (getenv(\"" + config.disable_env_var + "\")) return;");
            code.push_back("");
        }

        if (config.use_ptrace) {
            code.push_back("    // ptrace check");
            code.push_back("    if (ptrace(0, 0, 0, 0) == -1) _detected = 1;");
            code.push_back("    else ptrace(7, 0, 0, 0);  // Detach");
            code.push_back("");
        }

        if (config.use_tracer_pid) {
            code.push_back("    // TracerPid check");
            code.push_back("    {");
            code.push_back("        FILE* _f = fopen(\"/proc/self/status\", \"r\");");
            code.push_back("        if (_f) {");
            code.push_back("            char _line[256];");
            code.push_back("            while (fgets(_line, sizeof(_line), _f)) {");
            code.push_back("                if (strncmp(_line, \"TracerPid:\", 10) == 0) {");
            code.push_back("                    if (atoi(_line + 10) != 0) _detected = 1;");
            code.push_back("                    break;");
            code.push_back("                }");
            code.push_back("            }");
            code.push_back("            fclose(_f);");
            code.push_back("        }");
            code.push_back("    }");
            code.push_back("");
        }

        if (config.use_env_check) {
            code.push_back("    // Environment check");
            code.push_back("    if (getenv(\"LD_PRELOAD\") || getenv(\"LD_DEBUG\")) _detected = 1;");
            code.push_back("");
        }

        if (config.use_timing) {
            code.push_back("    // Timing check");
            code.push_back("    {");
            code.push_back("        struct timespec _ts1, _ts2;");
            code.push_back("        clock_gettime(CLOCK_MONOTONIC, &_ts1);");
            code.push_back("        volatile int _d = 0;");
            code.push_back("        for (int i = 0; i < 100; i++) _d += i;");
            code.push_back("        clock_gettime(CLOCK_MONOTONIC, &_ts2);");
            code.push_back("        uint64_t _diff = (_ts2.tv_sec - _ts1.tv_sec) * 1000000000ULL +");
            code.push_back("                         (_ts2.tv_nsec - _ts1.tv_nsec);");
            code.push_back("        if (_diff > " + std::to_string(config.timing_threshold_ns) + "ULL) _detected = 1;");
            code.push_back("    }");
            code.push_back("");
        }

        code.push_back("    if (_detected) _dbg_detected();");
        code.push_back("}");

        return code;
    }

private:
    static void appendResponse(std::vector<std::string>& code,
                               AntiDebugResponse response,
                               const std::string& indent) {
        switch (response) {
            case AntiDebugResponse::Exit:
                code.push_back(indent + "_exit(1);");
                break;

            case AntiDebugResponse::Crash:
                code.push_back(indent + "*(volatile int*)0 = 0;  // Crash");
                break;

            case AntiDebugResponse::InfiniteLoop:
                code.push_back(indent + "while(1) { }  // Hang");
                break;

            case AntiDebugResponse::CorruptData:
                code.push_back(indent + "// Corrupt internal state");
                code.push_back(indent + "extern volatile int _morphect_key;");
                code.push_back(indent + "_morphect_key ^= 0xDEADBEEF;");
                break;

            case AntiDebugResponse::ReturnWrong:
                code.push_back(indent + "return;  // Return early with wrong result");
                break;

            case AntiDebugResponse::Silent:
                code.push_back(indent + "extern volatile int _morphect_debug_detected;");
                code.push_back(indent + "_morphect_debug_detected = 1;");
                break;

            case AntiDebugResponse::Custom:
                code.push_back(indent + "// Custom response - user defined");
                break;
        }
    }

    static std::string encodeString(const std::string& s) {
        std::string result;
        for (char c : s) {
            char encoded = c ^ 0x42;
            char buf[8];
            snprintf(buf, sizeof(buf), "\\x%02x", (unsigned char)encoded);
            result += buf;
        }
        return result;
    }
};

// ============================================================================
// LLVM IR Anti-Debugging Generator
// ============================================================================

/**
 * Generates LLVM IR snippets for anti-debugging
 */
class LLVMAntiDebugGenerator {
public:
    /**
     * Generate LLVM IR for timing check
     */
    static std::vector<std::string> generateTimingCheck(
        AntiDebugResponse response,
        uint64_t threshold_ns,
        const std::string& fail_label) {

        std::vector<std::string> ir;

        ir.push_back("; Anti-debug: timing check");
        ir.push_back("  %_t1 = call i64 @llvm.readcyclecounter()");

        // Trivial work
        ir.push_back("  %_dummy = add i64 0, 1");
        ir.push_back("  %_dummy2 = mul i64 %_dummy, 2");

        ir.push_back("  %_t2 = call i64 @llvm.readcyclecounter()");
        ir.push_back("  %_diff = sub i64 %_t2, %_t1");

        // Threshold check (rough: ~300M cycles for 100ms at 3GHz)
        uint64_t cycle_threshold = (threshold_ns / 1000) * 3;
        ir.push_back("  %_slow = icmp ugt i64 %_diff, " +
                     std::to_string(cycle_threshold));
        ir.push_back("  br i1 %_slow, label %" + fail_label +
                     ", label %_timing_ok");
        ir.push_back("_timing_ok:");

        return ir;
    }

    /**
     * Generate LLVM IR for environment check (calls libc)
     */
    static std::vector<std::string> generateEnvCheck(
        const std::string& var_name,
        const std::string& fail_label) {

        std::vector<std::string> ir;

        ir.push_back("; Anti-debug: environment check for " + var_name);
        ir.push_back("  %_env_ptr = call i8* @getenv(i8* getelementptr ([" +
                     std::to_string(var_name.length() + 1) +
                     " x i8], [" + std::to_string(var_name.length() + 1) +
                     " x i8]* @.str_" + var_name + ", i64 0, i64 0))");
        ir.push_back("  %_env_null = icmp ne i8* %_env_ptr, null");
        ir.push_back("  br i1 %_env_null, label %" + fail_label +
                     ", label %_env_ok_" + var_name);
        ir.push_back("_env_ok_" + var_name + ":");

        return ir;
    }
};

// ============================================================================
// Assembly Anti-Debugging Generator
// ============================================================================

/**
 * Generates x86/x64 assembly for anti-debugging
 */
class X86AntiDebugGenerator {
public:
    /**
     * Generate inline assembly for RDTSC timing check
     */
    static std::vector<std::string> generateTimingCheckAsm(
        uint64_t threshold_cycles,
        const std::string& fail_label) {

        std::vector<std::string> asm_code;

        asm_code.push_back("    # Anti-debug: RDTSC timing check");
        asm_code.push_back("    rdtsc");
        asm_code.push_back("    shl rdx, 32");
        asm_code.push_back("    or rax, rdx");
        asm_code.push_back("    mov r15, rax           # Save start time");

        // Some trivial work
        asm_code.push_back("    xor ecx, ecx");
        asm_code.push_back("    mov eax, 100");
        asm_code.push_back(".Ltiming_loop:");
        asm_code.push_back("    add ecx, 1");
        asm_code.push_back("    cmp ecx, eax");
        asm_code.push_back("    jl .Ltiming_loop");

        asm_code.push_back("    rdtsc");
        asm_code.push_back("    shl rdx, 32");
        asm_code.push_back("    or rax, rdx");
        asm_code.push_back("    sub rax, r15           # Elapsed cycles");
        asm_code.push_back("    mov rcx, " + std::to_string(threshold_cycles));
        asm_code.push_back("    cmp rax, rcx");
        asm_code.push_back("    ja " + fail_label + "  # Jump if too slow");

        return asm_code;
    }

    /**
     * Generate inline assembly for int3 breakpoint detection
     * Checks if any breakpoints (0xCC bytes) are in the code
     */
    static std::vector<std::string> generateBreakpointCheckAsm(
        const std::string& start_label,
        const std::string& end_label,
        const std::string& fail_label) {

        std::vector<std::string> asm_code;

        asm_code.push_back("    # Anti-debug: breakpoint detection");
        asm_code.push_back("    lea rdi, [rip + " + start_label + "]");
        asm_code.push_back("    lea rsi, [rip + " + end_label + "]");
        asm_code.push_back(".Lbp_check_loop:");
        asm_code.push_back("    cmp rdi, rsi");
        asm_code.push_back("    jge .Lbp_check_done");
        asm_code.push_back("    movzx eax, byte ptr [rdi]");
        asm_code.push_back("    cmp al, 0xCC           # int3 opcode");
        asm_code.push_back("    je " + fail_label);
        asm_code.push_back("    inc rdi");
        asm_code.push_back("    jmp .Lbp_check_loop");
        asm_code.push_back(".Lbp_check_done:");

        return asm_code;
    }

    /**
     * Generate syscall-based ptrace check (no libc)
     */
    static std::vector<std::string> generatePtraceSyscallAsm(
        const std::string& fail_label) {

        std::vector<std::string> asm_code;

        asm_code.push_back("    # Anti-debug: ptrace syscall check");
        asm_code.push_back("    xor edi, edi           # PTRACE_TRACEME = 0");
        asm_code.push_back("    xor esi, esi           # pid = 0");
        asm_code.push_back("    xor edx, edx           # addr = 0");
        asm_code.push_back("    xor r10d, r10d         # data = 0");
        asm_code.push_back("    mov eax, 101           # __NR_ptrace (x86_64)");
        asm_code.push_back("    syscall");
        asm_code.push_back("    test rax, rax");
        asm_code.push_back("    js " + fail_label + "  # Jump if negative (error)");

        // Detach from self
        asm_code.push_back("    mov edi, 7             # PTRACE_DETACH");
        asm_code.push_back("    xor esi, esi");
        asm_code.push_back("    xor edx, edx");
        asm_code.push_back("    xor r10d, r10d");
        asm_code.push_back("    mov eax, 101");
        asm_code.push_back("    syscall");

        return asm_code;
    }
};

// ============================================================================
// Anti-Debug Analysis
// ============================================================================

/**
 * Analyzes code to find insertion points for anti-debug checks
 */
class AntiDebugAnalyzer {
public:
    /**
     * Find function entry points in LLVM IR
     */
    std::vector<int> findFunctionEntries(
        const std::vector<std::string>& ir_lines) const {

        std::vector<int> result;

        for (size_t i = 0; i < ir_lines.size(); i++) {
            const std::string& line = ir_lines[i];

            // Look for function definitions
            if (line.find("define ") != std::string::npos &&
                line.find("{") != std::string::npos) {
                // Find the entry block
                if (i + 1 < ir_lines.size()) {
                    result.push_back(static_cast<int>(i + 1));
                }
            }
        }

        return result;
    }

    /**
     * Find loop headers in LLVM IR
     */
    std::vector<int> findLoopHeaders(
        const std::vector<std::string>& ir_lines) const {

        std::vector<int> result;

        for (size_t i = 0; i < ir_lines.size(); i++) {
            const std::string& line = ir_lines[i];

            // Look for labels with loop-related names
            if (line.find("for.cond") != std::string::npos ||
                line.find("while.cond") != std::string::npos ||
                line.find("loop:") != std::string::npos ||
                line.find(".loop:") != std::string::npos) {
                result.push_back(static_cast<int>(i));
            }
        }

        return result;
    }

    /**
     * Find function entries in assembly
     */
    std::vector<int> findAsmFunctionEntries(
        const std::vector<std::string>& asm_lines) const {

        std::vector<int> result;

        for (size_t i = 0; i < asm_lines.size(); i++) {
            const std::string& line = asm_lines[i];

            // Look for function labels followed by prologue
            if (line.find(".type") != std::string::npos &&
                line.find("@function") != std::string::npos) {
                // Find the actual function label (next non-empty line ending with :)
                for (size_t j = i + 1; j < asm_lines.size() && j < i + 5; j++) {
                    std::string trimmed = trim(asm_lines[j]);
                    if (!trimmed.empty() && trimmed.back() == ':' &&
                        trimmed.find('.') == std::string::npos) {
                        // Next line after label is insertion point
                        if (j + 1 < asm_lines.size()) {
                            result.push_back(static_cast<int>(j + 1));
                        }
                        break;
                    }
                }
            }
        }

        return result;
    }

private:
    std::string trim(const std::string& s) const {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }
};

} // namespace antidebug
} // namespace morphect

#endif // MORPHECT_ANTIDEBUG_BASE_HPP
