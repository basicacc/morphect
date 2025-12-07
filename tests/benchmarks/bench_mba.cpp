/**
 * Morphect - MBA Transformation Benchmarks
 *
 * Measures the performance of MBA transformations.
 */

#include <benchmark/benchmark.h>
#include "passes/mba/mba.hpp"

using namespace morphect;
using namespace morphect::mba;

// ============================================================================
// Benchmark: LLVM IR Parsing and Transformation
// ============================================================================

static void BM_MBAAdd_ParseAndTransform(benchmark::State& state) {
    LLVMMBAAdd transform;
    MBAConfig config;
    config.enabled = true;
    config.probability = 1.0;

    std::string line = "  %result = add i32 %a, %b";

    for (auto _ : state) {
        auto result = transform.applyIR(line, 0, config);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MBAAdd_ParseAndTransform);

static void BM_MBAXor_ParseAndTransform(benchmark::State& state) {
    LLVMMBAXor transform;
    MBAConfig config;
    config.enabled = true;
    config.probability = 1.0;

    std::string line = "  %result = xor i32 %a, %b";

    for (auto _ : state) {
        auto result = transform.applyIR(line, 0, config);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MBAXor_ParseAndTransform);

static void BM_MBAAnd_ParseAndTransform(benchmark::State& state) {
    LLVMMBAAnd transform;
    MBAConfig config;
    config.enabled = true;
    config.probability = 1.0;

    std::string line = "  %result = and i32 %a, %b";

    for (auto _ : state) {
        auto result = transform.applyIR(line, 0, config);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MBAAnd_ParseAndTransform);

static void BM_MBAOr_ParseAndTransform(benchmark::State& state) {
    LLVMMBAOr transform;
    MBAConfig config;
    config.enabled = true;
    config.probability = 1.0;

    std::string line = "  %result = or i32 %a, %b";

    for (auto _ : state) {
        auto result = transform.applyIR(line, 0, config);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_MBAOr_ParseAndTransform);

// ============================================================================
// Benchmark: Variant Selection
// ============================================================================

static void BM_VariantSelection_Uniform(benchmark::State& state) {
    LLVMMBAAdd transform;
    MBAConfig config;
    config.enabled = true;
    config.probability = 1.0;

    for (auto _ : state) {
        size_t idx = transform.selectVariant(config);
        benchmark::DoNotOptimize(idx);
    }
}
BENCHMARK(BM_VariantSelection_Uniform);

static void BM_VariantSelection_Weighted(benchmark::State& state) {
    LLVMMBAAdd transform;
    MBAConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.variant_weights = {0.4, 0.3, 0.2, 0.1};

    for (auto _ : state) {
        size_t idx = transform.selectVariant(config);
        benchmark::DoNotOptimize(idx);
    }
}
BENCHMARK(BM_VariantSelection_Weighted);

// ============================================================================
// Benchmark: Random Number Generation
// ============================================================================

static void BM_RandomDecide(benchmark::State& state) {
    for (auto _ : state) {
        bool result = GlobalRandom::decide(0.85);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RandomDecide);

static void BM_RandomNextInt(benchmark::State& state) {
    for (auto _ : state) {
        int result = GlobalRandom::nextInt(0, 1000);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RandomNextInt);

// ============================================================================
// Benchmark: Full Pass
// ============================================================================

static void BM_LLVMMBAPass_SmallFunction(benchmark::State& state) {
    LLVMMBAPass pass;
    MBAPassConfig config;
    config.global.enabled = true;
    config.global.probability = 1.0;
    pass.initializeMBA(config);

    // Small function with a few operations
    std::vector<std::string> lines = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %sum = add i32 %a, %b",
        "  %xor = xor i32 %sum, %a",
        "  %and = and i32 %xor, %b",
        "  ret i32 %and",
        "}"
    };

    for (auto _ : state) {
        std::vector<std::string> copy = lines;
        pass.transformIR(copy);
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_LLVMMBAPass_SmallFunction);

static void BM_LLVMMBAPass_MediumFunction(benchmark::State& state) {
    LLVMMBAPass pass;
    MBAPassConfig config;
    config.global.enabled = true;
    config.global.probability = 1.0;
    pass.initializeMBA(config);

    // Medium function with more operations
    std::vector<std::string> lines = {
        "define i32 @compute(i32 %a, i32 %b, i32 %c) {",
        "entry:",
        "  %t1 = add i32 %a, %b",
        "  %t2 = sub i32 %t1, %c",
        "  %t3 = xor i32 %t2, %a",
        "  %t4 = and i32 %t3, %b",
        "  %t5 = or i32 %t4, %c",
        "  %t6 = add i32 %t5, %t1",
        "  %t7 = xor i32 %t6, %t2",
        "  %t8 = and i32 %t7, %t3",
        "  ret i32 %t8",
        "}"
    };

    for (auto _ : state) {
        std::vector<std::string> copy = lines;
        pass.transformIR(copy);
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_LLVMMBAPass_MediumFunction);

// ============================================================================
// Benchmark: CFF Analysis
// ============================================================================

#include "passes/cff/cff.hpp"

using namespace morphect::cff;

static void BM_CFGAnalysis_SimpleFunction(benchmark::State& state) {
    LLVMCFGAnalyzer analyzer;

    std::vector<std::string> ir = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %sum = add i32 %a, %b",
        "  ret i32 %sum",
        "}"
    };

    for (auto _ : state) {
        auto result = analyzer.analyze(ir);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_CFGAnalysis_SimpleFunction);

static void BM_CFGAnalysis_ConditionalFunction(benchmark::State& state) {
    LLVMCFGAnalyzer analyzer;

    std::vector<std::string> ir = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %cmp = icmp sgt i32 %a, %b",
        "  br i1 %cmp, label %then, label %else",
        "then:",
        "  %t1 = add i32 %a, 1",
        "  br label %end",
        "else:",
        "  %t2 = add i32 %b, 1",
        "  br label %end",
        "end:",
        "  %result = phi i32 [ %t1, %then ], [ %t2, %else ]",
        "  ret i32 %result",
        "}"
    };

    for (auto _ : state) {
        auto result = analyzer.analyze(ir);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_CFGAnalysis_ConditionalFunction);

static void BM_CFGAnalysis_LoopFunction(benchmark::State& state) {
    LLVMCFGAnalyzer analyzer;

    std::vector<std::string> ir = {
        "define i32 @sum(i32 %n) {",
        "entry:",
        "  br label %loop",
        "loop:",
        "  %i = phi i32 [ 0, %entry ], [ %next_i, %loop ]",
        "  %sum = phi i32 [ 0, %entry ], [ %next_sum, %loop ]",
        "  %next_sum = add i32 %sum, %i",
        "  %next_i = add i32 %i, 1",
        "  %cond = icmp slt i32 %next_i, %n",
        "  br i1 %cond, label %loop, label %exit",
        "exit:",
        "  ret i32 %next_sum",
        "}"
    };

    for (auto _ : state) {
        auto result = analyzer.analyze(ir);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_CFGAnalysis_LoopFunction);

// ============================================================================
// Benchmark: CFF Transformation
// ============================================================================

static void BM_CFFTransform_SimpleFunction(benchmark::State& state) {
    LLVMCFGAnalyzer analyzer;
    LLVMCFFTransformation transformer;
    CFFConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.shuffle_states = false;

    std::vector<std::string> ir = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %sum = add i32 %a, %b",
        "  br label %exit",
        "exit:",
        "  ret i32 %sum",
        "}"
    };

    auto cfg = analyzer.analyze(ir);

    for (auto _ : state) {
        auto result = transformer.flatten(cfg.value(), config);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_CFFTransform_SimpleFunction);

static void BM_CFFTransform_ConditionalFunction(benchmark::State& state) {
    LLVMCFGAnalyzer analyzer;
    LLVMCFFTransformation transformer;
    CFFConfig config;
    config.enabled = true;
    config.probability = 1.0;
    config.shuffle_states = false;

    std::vector<std::string> ir = {
        "define i32 @test(i32 %a, i32 %b) {",
        "entry:",
        "  %cmp = icmp sgt i32 %a, %b",
        "  br i1 %cmp, label %then, label %else",
        "then:",
        "  %t1 = add i32 %a, 1",
        "  br label %end",
        "else:",
        "  %t2 = add i32 %b, 1",
        "  br label %end",
        "end:",
        "  ret i32 0",
        "}"
    };

    auto cfg = analyzer.analyze(ir);

    for (auto _ : state) {
        auto result = transformer.flatten(cfg.value(), config);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_CFFTransform_ConditionalFunction);

// ============================================================================
// Benchmark: Opaque Predicates
// ============================================================================

static void BM_OpaquePredicateGeneration(benchmark::State& state) {
    OpaquePredicateLibrary predicates;

    for (auto _ : state) {
        auto [code, var] = predicates.generateAlwaysTrue("%x", "%y");
        benchmark::DoNotOptimize(code);
        benchmark::DoNotOptimize(var);
    }
}
BENCHMARK(BM_OpaquePredicateGeneration);

static void BM_OpaquePredicateSelection(benchmark::State& state) {
    OpaquePredicateLibrary predicates;

    for (auto _ : state) {
        auto pred = predicates.getAlwaysTrue();
        benchmark::DoNotOptimize(pred);
    }
}
BENCHMARK(BM_OpaquePredicateSelection);

// ============================================================================
// Benchmark: Dead Code Generation
// ============================================================================

static void BM_DeadCodeGeneration(benchmark::State& state) {
    DeadCodeGenerator generator;
    std::vector<std::string> vars = {"%a", "%b", "%c", "%d"};

    for (auto _ : state) {
        auto code = generator.generateLLVM(vars, 10);
        benchmark::DoNotOptimize(code);
    }
}
BENCHMARK(BM_DeadCodeGeneration);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
