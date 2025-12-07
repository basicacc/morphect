/**
 * Morphect - Anti-Debugging Tests
 *
 * Tests for anti-debugging transformations
 */

#include <gtest/gtest.h>
#include "passes/antidebug/antidebug.hpp"

using namespace morphect;
using namespace morphect::antidebug;

// ============================================================================
// Configuration Tests
// ============================================================================

TEST(AntiDebugTest, Config_DefaultValues) {
    AntiDebugConfig config;

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.target_os, TargetOS::Linux);
    EXPECT_EQ(config.response, AntiDebugResponse::Exit);
    EXPECT_TRUE(config.use_ptrace);
    EXPECT_TRUE(config.use_timing);
    EXPECT_TRUE(config.use_env_check);
    EXPECT_TRUE(config.use_parent_check);
    EXPECT_TRUE(config.use_tracer_pid);
    EXPECT_FALSE(config.use_breakpoint_check);
    EXPECT_FALSE(config.use_hardware_bp_check);
    EXPECT_EQ(config.timing_threshold_ns, 100000000ULL);
    EXPECT_EQ(config.timing_samples, 3);
    EXPECT_TRUE(config.obfuscate_checks);
    EXPECT_FALSE(config.allow_disable_via_env);
    EXPECT_DOUBLE_EQ(config.probability, 0.3);
    EXPECT_EQ(config.max_checks_per_function, 2);
    EXPECT_TRUE(config.insert_at_entry);
    EXPECT_FALSE(config.insert_at_loops);
    EXPECT_TRUE(config.insert_random);
}

TEST(AntiDebugTest, Config_CustomValues) {
    AntiDebugConfig config;
    config.enabled = false;
    config.target_os = TargetOS::Windows;
    config.response = AntiDebugResponse::Crash;
    config.use_ptrace = false;
    config.timing_threshold_ns = 50000000;
    config.probability = 0.8;

    EXPECT_FALSE(config.enabled);
    EXPECT_EQ(config.target_os, TargetOS::Windows);
    EXPECT_EQ(config.response, AntiDebugResponse::Crash);
    EXPECT_FALSE(config.use_ptrace);
    EXPECT_EQ(config.timing_threshold_ns, 50000000ULL);
    EXPECT_DOUBLE_EQ(config.probability, 0.8);
}

// ============================================================================
// Linux Anti-Debug Generator Tests
// ============================================================================

TEST(AntiDebugTest, Linux_GeneratePtraceCheck) {
    auto code = LinuxAntiDebugGenerator::generatePtraceCheck(
        AntiDebugResponse::Exit, false);

    EXPECT_FALSE(code.empty());

    // Check for key elements
    bool has_include = false;
    bool has_ptrace = false;
    bool has_exit = false;

    for (const auto& line : code) {
        if (line.find("sys/ptrace.h") != std::string::npos) has_include = true;
        if (line.find("ptrace(PTRACE_TRACEME") != std::string::npos) has_ptrace = true;
        if (line.find("_exit") != std::string::npos) has_exit = true;
    }

    EXPECT_TRUE(has_include);
    EXPECT_TRUE(has_ptrace);
    EXPECT_TRUE(has_exit);
}

TEST(AntiDebugTest, Linux_GeneratePtraceCheck_Obfuscated) {
    auto code = LinuxAntiDebugGenerator::generatePtraceCheck(
        AntiDebugResponse::Exit, true);

    EXPECT_FALSE(code.empty());

    // Obfuscated version uses indirect call
    bool has_indirect_call = false;
    for (const auto& line : code) {
        if (line.find("_pt)(int") != std::string::npos) has_indirect_call = true;
    }

    EXPECT_TRUE(has_indirect_call);
}

TEST(AntiDebugTest, Linux_GenerateTimingCheck_RDTSC) {
    auto code = LinuxAntiDebugGenerator::generateTimingCheck(
        AntiDebugResponse::Exit, 100000000, true, false);

    EXPECT_FALSE(code.empty());

    // Check for RDTSC
    bool has_rdtsc = false;
    for (const auto& line : code) {
        if (line.find("rdtsc") != std::string::npos) has_rdtsc = true;
    }

    EXPECT_TRUE(has_rdtsc);
}

TEST(AntiDebugTest, Linux_GenerateTimingCheck_ClockGettime) {
    auto code = LinuxAntiDebugGenerator::generateTimingCheck(
        AntiDebugResponse::Exit, 100000000, false, false);

    EXPECT_FALSE(code.empty());

    // Check for clock_gettime
    bool has_clock = false;
    for (const auto& line : code) {
        if (line.find("clock_gettime") != std::string::npos) has_clock = true;
    }

    EXPECT_TRUE(has_clock);
}

TEST(AntiDebugTest, Linux_GenerateEnvCheck) {
    auto code = LinuxAntiDebugGenerator::generateEnvCheck(
        AntiDebugResponse::Exit, false);

    EXPECT_FALSE(code.empty());

    // Check for environment variable checks
    bool has_getenv = false;
    bool has_ld_preload = false;

    for (const auto& line : code) {
        if (line.find("getenv") != std::string::npos) has_getenv = true;
        if (line.find("LD_PRELOAD") != std::string::npos) has_ld_preload = true;
    }

    EXPECT_TRUE(has_getenv);
    EXPECT_TRUE(has_ld_preload);
}

TEST(AntiDebugTest, Linux_GenerateTracerPidCheck) {
    auto code = LinuxAntiDebugGenerator::generateTracerPidCheck(
        AntiDebugResponse::Exit, false);

    EXPECT_FALSE(code.empty());

    // Check for /proc/self/status reading
    bool has_proc = false;
    bool has_tracer = false;

    for (const auto& line : code) {
        if (line.find("/proc/self/status") != std::string::npos) has_proc = true;
        if (line.find("TracerPid") != std::string::npos) has_tracer = true;
    }

    EXPECT_TRUE(has_proc);
    EXPECT_TRUE(has_tracer);
}

TEST(AntiDebugTest, Linux_GenerateParentCheck) {
    auto code = LinuxAntiDebugGenerator::generateParentCheck(
        AntiDebugResponse::Exit, false);

    EXPECT_FALSE(code.empty());

    // Check for parent process check
    bool has_getppid = false;
    bool has_gdb = false;

    for (const auto& line : code) {
        if (line.find("getppid") != std::string::npos) has_getppid = true;
        if (line.find("gdb") != std::string::npos) has_gdb = true;
    }

    EXPECT_TRUE(has_getppid);
    EXPECT_TRUE(has_gdb);
}

TEST(AntiDebugTest, Linux_GenerateCombinedCheck) {
    AntiDebugConfig config;
    config.use_ptrace = true;
    config.use_timing = true;
    config.use_tracer_pid = true;
    config.use_env_check = true;
    config.obfuscate_checks = false;

    auto code = LinuxAntiDebugGenerator::generateCombinedCheck(config);

    EXPECT_FALSE(code.empty());

    // Check for combined elements
    bool has_ptrace = false;
    bool has_timing = false;
    bool has_env = false;
    bool has_detected = false;

    for (const auto& line : code) {
        if (line.find("ptrace") != std::string::npos) has_ptrace = true;
        if (line.find("clock_gettime") != std::string::npos) has_timing = true;
        if (line.find("getenv") != std::string::npos) has_env = true;
        if (line.find("_detected") != std::string::npos) has_detected = true;
    }

    EXPECT_TRUE(has_ptrace);
    EXPECT_TRUE(has_timing);
    EXPECT_TRUE(has_env);
    EXPECT_TRUE(has_detected);
}

// ============================================================================
// Response Type Tests
// ============================================================================

TEST(AntiDebugTest, Response_Exit) {
    auto code = LinuxAntiDebugGenerator::generatePtraceCheck(
        AntiDebugResponse::Exit, false);

    bool has_exit = false;
    for (const auto& line : code) {
        if (line.find("_exit") != std::string::npos) has_exit = true;
    }
    EXPECT_TRUE(has_exit);
}

TEST(AntiDebugTest, Response_Crash) {
    auto code = LinuxAntiDebugGenerator::generatePtraceCheck(
        AntiDebugResponse::Crash, false);

    bool has_crash = false;
    for (const auto& line : code) {
        if (line.find("*(volatile int*)0") != std::string::npos) has_crash = true;
    }
    EXPECT_TRUE(has_crash);
}

TEST(AntiDebugTest, Response_InfiniteLoop) {
    auto code = LinuxAntiDebugGenerator::generatePtraceCheck(
        AntiDebugResponse::InfiniteLoop, false);

    bool has_loop = false;
    for (const auto& line : code) {
        if (line.find("while(1)") != std::string::npos) has_loop = true;
    }
    EXPECT_TRUE(has_loop);
}

TEST(AntiDebugTest, Response_Silent) {
    auto code = LinuxAntiDebugGenerator::generatePtraceCheck(
        AntiDebugResponse::Silent, false);

    bool has_flag = false;
    for (const auto& line : code) {
        if (line.find("_morphect_debug_detected") != std::string::npos) has_flag = true;
    }
    EXPECT_TRUE(has_flag);
}

// ============================================================================
// X86 Assembly Generator Tests
// ============================================================================

TEST(AntiDebugTest, X86_GenerateTimingCheckAsm) {
    auto code = X86AntiDebugGenerator::generateTimingCheckAsm(
        300000000, ".Lfail");

    EXPECT_FALSE(code.empty());

    // Check for RDTSC
    bool has_rdtsc = false;
    bool has_cmp = false;
    bool has_jump = false;

    for (const auto& line : code) {
        if (line.find("rdtsc") != std::string::npos) has_rdtsc = true;
        if (line.find("cmp") != std::string::npos) has_cmp = true;
        if (line.find("ja .Lfail") != std::string::npos) has_jump = true;
    }

    EXPECT_TRUE(has_rdtsc);
    EXPECT_TRUE(has_cmp);
    EXPECT_TRUE(has_jump);
}

TEST(AntiDebugTest, X86_GeneratePtraceSyscallAsm) {
    auto code = X86AntiDebugGenerator::generatePtraceSyscallAsm(".Lfail");

    EXPECT_FALSE(code.empty());

    // Check for syscall
    bool has_syscall = false;
    bool has_ptrace_nr = false;
    bool has_jump = false;

    for (const auto& line : code) {
        if (line.find("syscall") != std::string::npos) has_syscall = true;
        if (line.find("101") != std::string::npos) has_ptrace_nr = true;  // __NR_ptrace
        if (line.find("js .Lfail") != std::string::npos) has_jump = true;
    }

    EXPECT_TRUE(has_syscall);
    EXPECT_TRUE(has_ptrace_nr);
    EXPECT_TRUE(has_jump);
}

TEST(AntiDebugTest, X86_GenerateBreakpointCheckAsm) {
    auto code = X86AntiDebugGenerator::generateBreakpointCheckAsm(
        ".Lstart", ".Lend", ".Lfail");

    EXPECT_FALSE(code.empty());

    // Check for int3 detection
    bool has_int3 = false;
    bool has_loop = false;

    for (const auto& line : code) {
        if (line.find("0xCC") != std::string::npos) has_int3 = true;
        if (line.find("bp_check_loop") != std::string::npos) has_loop = true;
    }

    EXPECT_TRUE(has_int3);
    EXPECT_TRUE(has_loop);
}

// ============================================================================
// LLVM IR Generator Tests
// ============================================================================

TEST(AntiDebugTest, LLVM_GenerateTimingCheck) {
    auto code = LLVMAntiDebugGenerator::generateTimingCheck(
        AntiDebugResponse::Exit, 100000000, "fail_label");

    EXPECT_FALSE(code.empty());

    // Check for readcyclecounter
    bool has_counter = false;
    bool has_icmp = false;

    for (const auto& line : code) {
        if (line.find("readcyclecounter") != std::string::npos) has_counter = true;
        if (line.find("icmp ugt") != std::string::npos) has_icmp = true;
    }

    EXPECT_TRUE(has_counter);
    EXPECT_TRUE(has_icmp);
}

TEST(AntiDebugTest, LLVM_GenerateEnvCheck) {
    auto code = LLVMAntiDebugGenerator::generateEnvCheck(
        "LD_PRELOAD", "fail_label");

    EXPECT_FALSE(code.empty());

    // Check for getenv call
    bool has_getenv = false;
    bool has_icmp = false;

    for (const auto& line : code) {
        if (line.find("@getenv") != std::string::npos) has_getenv = true;
        if (line.find("icmp ne") != std::string::npos) has_icmp = true;
    }

    EXPECT_TRUE(has_getenv);
    EXPECT_TRUE(has_icmp);
}

// ============================================================================
// Assembly Anti-Debug Transformation Tests
// ============================================================================

TEST(AntiDebugTest, AsmTransform_Disabled) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = false;

    std::vector<std::string> lines = {
        "    .text",
        "    .globl main",
        "main:",
        "    push %rbp",
        "    mov %rsp, %rbp",
        "    ret"
    };

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.generated_code.size(), lines.size());
    EXPECT_EQ(result.checks_inserted, 0);
}

TEST(AntiDebugTest, AsmTransform_InsertsChecks) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.use_timing = true;
    config.use_ptrace = true;
    config.insert_at_entry = true;

    std::vector<std::string> lines = {
        "    .text",
        "    .globl main",
        "    .type main, @function",
        "main:",
        "    push %rbp",
        "    mov %rsp, %rbp",
        "    xor %eax, %eax",
        "    pop %rbp",
        "    ret",
        "    .size main, .-main"
    };

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.generated_code.size(), lines.size());
    EXPECT_GT(result.checks_inserted, 0);

    // Check for anti-debug code
    bool has_antidebug = false;
    for (const auto& line : result.generated_code) {
        if (line.find("Anti-debug") != std::string::npos) has_antidebug = true;
    }
    EXPECT_TRUE(has_antidebug);
}

TEST(AntiDebugTest, AsmTransform_IncludesFailHandler) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.response = AntiDebugResponse::Exit;

    std::vector<std::string> lines = {
        "    .text",
        "    .globl main",
        "    .type main, @function",
        "main:",
        "    push %rbp",
        "    ret",
        "    .size main, .-main"
    };

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);

    // Check for fail handler
    bool has_fail_handler = false;
    bool has_exit_syscall = false;

    for (const auto& line : result.generated_code) {
        if (line.find("antidebug_fail") != std::string::npos) has_fail_handler = true;
        if (line.find("__NR_exit") != std::string::npos) has_exit_syscall = true;
    }

    if (result.checks_inserted > 0) {
        EXPECT_TRUE(has_fail_handler);
        EXPECT_TRUE(has_exit_syscall);
    }
}

// ============================================================================
// Assembly Anti-Debug Pass Tests
// ============================================================================

TEST(AntiDebugTest, AsmPass_Initialize) {
    AssemblyAntiDebugPass pass;

    PassConfig config;
    config.enabled = true;
    config.probability = 0.5;

    EXPECT_TRUE(pass.initialize(config));
    EXPECT_EQ(pass.getName(), "AntiDebug");
    EXPECT_EQ(pass.getPriority(), PassPriority::Late);
}

TEST(AntiDebugTest, AsmPass_Transform) {
    AssemblyAntiDebugPass pass;

    AntiDebugConfig ad_config;
    ad_config.enabled = true;
    ad_config.probability = 1.0;
    ad_config.target_os = TargetOS::Linux;

    pass.setAntiDebugConfig(ad_config);

    std::vector<std::string> lines = {
        "    .text",
        "    .globl func",
        "    .type func, @function",
        "func:",
        "    push %rbp",
        "    mov %rsp, %rbp",
        "    ret",
        "    .size func, .-func"
    };

    auto result = pass.transformAssembly(lines, "x86_64");

    EXPECT_EQ(result, TransformResult::Success);
}

TEST(AntiDebugTest, AsmPass_SkipsWhenDisabled) {
    AssemblyAntiDebugPass pass;

    AntiDebugConfig ad_config;
    ad_config.enabled = false;

    pass.setAntiDebugConfig(ad_config);

    std::vector<std::string> lines = {"    push %rbp"};

    auto result = pass.transformAssembly(lines, "x86_64");

    EXPECT_EQ(result, TransformResult::Skipped);
}

TEST(AntiDebugTest, AsmPass_Statistics) {
    AssemblyAntiDebugPass pass;

    AntiDebugConfig ad_config;
    ad_config.enabled = true;
    ad_config.probability = 1.0;

    pass.setAntiDebugConfig(ad_config);

    std::vector<std::string> lines = {
        "    .text",
        "    .type main, @function",
        "main:",
        "    push %rbp",
        "    ret",
        "    .size main, .-main"
    };

    pass.transformAssembly(lines, "x86_64");

    auto stats = pass.getStatistics();
    // Should have checks_inserted key
    EXPECT_TRUE(stats.find("checks_inserted") != stats.end());
}

// ============================================================================
// LLVM Anti-Debug Pass Tests
// ============================================================================

TEST(AntiDebugTest, LLVMPass_Initialize) {
    LLVMAntiDebugPass pass;

    PassConfig config;
    config.enabled = true;
    config.probability = 0.5;

    EXPECT_TRUE(pass.initialize(config));
    EXPECT_EQ(pass.getName(), "AntiDebug");
}

TEST(AntiDebugTest, LLVMPass_SkipsWhenDisabled) {
    LLVMAntiDebugPass pass;

    AntiDebugConfig ad_config;
    ad_config.enabled = false;

    pass.setAntiDebugConfig(ad_config);
    pass.setTargetTriple("x86_64-linux-gnu");

    std::vector<std::string> lines = {"define i32 @main() {"};

    auto result = pass.transformIR(lines);

    EXPECT_EQ(result, TransformResult::Skipped);
}

// ============================================================================
// C Generator Tests
// ============================================================================

TEST(AntiDebugTest, CGenerator_GenerateHeader) {
    AntiDebugConfig config;
    config.use_ptrace = true;
    config.use_timing = true;
    config.obfuscate_checks = false;

    auto code = CAntiDebugGenerator::generateHeader(config);

    EXPECT_FALSE(code.empty());

    // Check for header guards
    bool has_ifndef = false;
    bool has_endif = false;
    bool has_function = false;

    for (const auto& line : code) {
        if (line.find("#ifndef MORPHECT_ANTIDEBUG_H") != std::string::npos) has_ifndef = true;
        if (line.find("#endif") != std::string::npos) has_endif = true;
        if (line.find("morphect_antidebug_check") != std::string::npos) has_function = true;
    }

    EXPECT_TRUE(has_ifndef);
    EXPECT_TRUE(has_endif);
    EXPECT_TRUE(has_function);
}

TEST(AntiDebugTest, CGenerator_GenerateTechnique_Ptrace) {
    AntiDebugConfig config;
    config.response = AntiDebugResponse::Exit;
    config.obfuscate_checks = false;

    auto code = CAntiDebugGenerator::generateTechnique(
        AntiDebugTechnique::PtraceDetection, config);

    EXPECT_FALSE(code.empty());

    bool has_ptrace = false;
    for (const auto& line : code) {
        if (line.find("ptrace") != std::string::npos) has_ptrace = true;
    }
    EXPECT_TRUE(has_ptrace);
}

TEST(AntiDebugTest, CGenerator_GenerateTechnique_Timing) {
    AntiDebugConfig config;
    config.response = AntiDebugResponse::Exit;
    config.obfuscate_checks = false;

    auto code = CAntiDebugGenerator::generateTechnique(
        AntiDebugTechnique::TimingCheck, config);

    EXPECT_FALSE(code.empty());

    bool has_timing = false;
    for (const auto& line : code) {
        if (line.find("rdtsc") != std::string::npos ||
            line.find("clock_gettime") != std::string::npos) {
            has_timing = true;
        }
    }
    EXPECT_TRUE(has_timing);
}

// ============================================================================
// Anti-Debug Analyzer Tests
// ============================================================================

TEST(AntiDebugTest, Analyzer_FindFunctionEntries) {
    AntiDebugAnalyzer analyzer;

    std::vector<std::string> ir = {
        "; ModuleID = 'test'",
        "define i32 @main() {",
        "entry:",
        "  ret i32 0",
        "}",
        "",
        "define void @helper() {",
        "entry:",
        "  ret void",
        "}"
    };

    auto entries = analyzer.findFunctionEntries(ir);

    EXPECT_EQ(entries.size(), 2);
}

TEST(AntiDebugTest, Analyzer_FindLoopHeaders) {
    AntiDebugAnalyzer analyzer;

    std::vector<std::string> ir = {
        "define i32 @main() {",
        "entry:",
        "  br label %for.cond",
        "for.cond:",
        "  br i1 %cmp, label %for.body, label %for.end",
        "for.body:",
        "  br label %for.cond",
        "for.end:",
        "  ret i32 0",
        "}"
    };

    auto loops = analyzer.findLoopHeaders(ir);

    EXPECT_GE(loops.size(), 1);
}

TEST(AntiDebugTest, Analyzer_FindAsmFunctionEntries) {
    AntiDebugAnalyzer analyzer;

    std::vector<std::string> asm_lines = {
        "    .text",
        "    .globl main",
        "    .type main, @function",
        "main:",
        "    push %rbp",
        "    ret",
        "    .size main, .-main"
    };

    auto entries = analyzer.findAsmFunctionEntries(asm_lines);

    EXPECT_EQ(entries.size(), 1);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(AntiDebugTest, EdgeCase_EmptyInput) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = true;

    std::vector<std::string> lines;

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.checks_inserted, 0);
}

TEST(AntiDebugTest, EdgeCase_NoFunctions) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = true;

    std::vector<std::string> lines = {
        "    .data",
        "    .globl var",
        "var:",
        "    .long 42"
    };

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.checks_inserted, 0);
}

TEST(AntiDebugTest, EdgeCase_ZeroProbability) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = true;
    config.probability = 0.0;

    std::vector<std::string> lines = {
        "    .type main, @function",
        "main:",
        "    ret",
        "    .size main, .-main"
    };

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.checks_inserted, 0);
}

TEST(AntiDebugTest, EdgeCase_MaxChecksPerFunction) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.max_checks_per_function = 1;

    std::vector<std::string> lines = {
        "    .type main, @function",
        "main:",
        "    push %rbp",
        "    ret",
        "    .size main, .-main",
        "    .type other, @function",
        "other:",
        "    push %rbp",
        "    ret",
        "    .size other, .-other"
    };

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);
    // Should have at most 2 checks (1 per function)
    EXPECT_LE(result.checks_inserted, 2);
}

TEST(AntiDebugTest, EdgeCase_AllowDisableViaEnv) {
    AntiDebugConfig config;
    config.allow_disable_via_env = true;
    config.disable_env_var = "MY_NODEBUG";

    auto code = LinuxAntiDebugGenerator::generateCombinedCheck(config);

    bool has_env_check = false;
    for (const auto& line : code) {
        if (line.find("MY_NODEBUG") != std::string::npos) has_env_check = true;
    }
    EXPECT_TRUE(has_env_check);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(AntiDebugTest, Integration_AllTechniques) {
    AntiDebugConfig config;
    config.use_ptrace = true;
    config.use_timing = true;
    config.use_env_check = true;
    config.use_tracer_pid = true;
    config.use_parent_check = true;
    config.obfuscate_checks = false;

    auto code = LinuxAntiDebugGenerator::generateCombinedCheck(config);

    EXPECT_FALSE(code.empty());

    // Should have all techniques
    bool has_ptrace = false;
    bool has_timing = false;
    bool has_env = false;
    bool has_tracer = false;

    for (const auto& line : code) {
        if (line.find("ptrace") != std::string::npos) has_ptrace = true;
        if (line.find("clock_gettime") != std::string::npos) has_timing = true;
        if (line.find("getenv") != std::string::npos) has_env = true;
        if (line.find("TracerPid") != std::string::npos) has_tracer = true;
    }

    EXPECT_TRUE(has_ptrace);
    EXPECT_TRUE(has_timing);
    EXPECT_TRUE(has_env);
    EXPECT_TRUE(has_tracer);
}

TEST(AntiDebugTest, Integration_AsmWithMultipleFunctions) {
    AssemblyAntiDebugTransformation transformer;
    AntiDebugConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.max_checks_per_function = 1;

    std::vector<std::string> lines = {
        "    .text",
        "    .type func1, @function",
        "func1:",
        "    push %rbp",
        "    ret",
        "    .size func1, .-func1",
        "    .type func2, @function",
        "func2:",
        "    push %rbp",
        "    ret",
        "    .size func2, .-func2",
        "    .type func3, @function",
        "func3:",
        "    push %rbp",
        "    ret",
        "    .size func3, .-func3"
    };

    auto result = transformer.transform(lines, config);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.generated_code.size(), lines.size());
}
