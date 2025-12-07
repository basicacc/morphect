/**
 * Morphect - LLVM IR Obfuscator Integration Tests
 *
 * Tests that IR obfuscation produces correct output.
 */

#include <gtest/gtest.h>
#include "../fixtures/obfuscation_fixture.hpp"

using namespace morphect::test;

class IRIntegrationTest : public LLVMIRFixture {
protected:
    void SetUp() override {
        LLVMIRFixture::SetUp();

        // Skip tests if IR obfuscator doesn't exist
        if (!std::filesystem::exists(ir_obf_path_)) {
            GTEST_SKIP() << "IR obfuscator not found at: " << ir_obf_path_;
        }
    }
};

// ============================================================================
// MBA Transformation Tests
// ============================================================================

TEST_F(IRIntegrationTest, TransformAdd) {
    const char* ir = R"(
define i32 @add(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 1.0,
            "operations": ["add"]
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // Should have XOR and AND operations from MBA
    EXPECT_TRUE(irContains(obfuscated, "xor") || irContains(obfuscated, "and"));
}

TEST_F(IRIntegrationTest, TransformXor) {
    const char* ir = R"(
define i32 @xor_func(i32 %a, i32 %b) {
entry:
  %result = xor i32 %a, %b
  ret i32 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 1.0,
            "operations": ["xor"]
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // XOR should be replaced with OR/AND combination
    EXPECT_TRUE(irContains(obfuscated, "or") || irContains(obfuscated, "and"));
}

TEST_F(IRIntegrationTest, TransformAnd) {
    const char* ir = R"(
define i32 @and_func(i32 %a, i32 %b) {
entry:
  %result = and i32 %a, %b
  ret i32 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 1.0,
            "operations": ["and"]
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // AND should be replaced with OR/XOR combination
    EXPECT_TRUE(irContains(obfuscated, "or") || irContains(obfuscated, "sub"));
}

TEST_F(IRIntegrationTest, TransformOr) {
    const char* ir = R"(
define i32 @or_func(i32 %a, i32 %b) {
entry:
  %result = or i32 %a, %b
  ret i32 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 1.0,
            "operations": ["or"]
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // OR should be replaced with XOR/AND combination
    EXPECT_TRUE(irContains(obfuscated, "xor") || irContains(obfuscated, "and"));
}

// ============================================================================
// Probability Tests
// ============================================================================

TEST_F(IRIntegrationTest, ZeroProbability) {
    const char* ir = R"(
define i32 @test(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 0.0
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // With 0 probability, should not transform
    EXPECT_TRUE(irContains(obfuscated, "add i32 %a, %b") ||
                irContains(obfuscated, "add nsw i32 %a, %b"));
}

TEST_F(IRIntegrationTest, DisabledPass) {
    const char* ir = R"(
define i32 @test(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": false
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // Should be unchanged when disabled
    EXPECT_TRUE(irContains(obfuscated, "add i32") ||
                irContains(obfuscated, "add nsw i32"));
}

// ============================================================================
// Complex Function Tests
// ============================================================================

TEST_F(IRIntegrationTest, MultipleBBFunction) {
    const char* ir = R"(
define i32 @max(i32 %a, i32 %b) {
entry:
  %cmp = icmp sgt i32 %a, %b
  br i1 %cmp, label %then, label %else

then:
  %t1 = add i32 %a, 1
  br label %end

else:
  %t2 = add i32 %b, 1
  br label %end

end:
  %result = phi i32 [ %t1, %then ], [ %t2, %else ]
  ret i32 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 1.0
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // Should still have basic structure
    EXPECT_TRUE(irContains(obfuscated, "define"));
    EXPECT_TRUE(irContains(obfuscated, "ret"));
}

TEST_F(IRIntegrationTest, LoopFunction) {
    const char* ir = R"(
define i32 @sum(i32 %n) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %next_i, %loop ]
  %sum = phi i32 [ 0, %entry ], [ %next_sum, %loop ]
  %next_sum = add i32 %sum, %i
  %next_i = add i32 %i, 1
  %cond = icmp slt i32 %next_i, %n
  br i1 %cond, label %loop, label %exit

exit:
  ret i32 %next_sum
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 1.0
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // Should preserve phi nodes
    EXPECT_TRUE(irContains(obfuscated, "phi"));
}

// ============================================================================
// Data Type Tests
// ============================================================================

TEST_F(IRIntegrationTest, DifferentTypes) {
    const char* ir = R"(
define i64 @test64(i64 %a, i64 %b) {
entry:
  %result = add i64 %a, %b
  ret i64 %result
}

define i16 @test16(i16 %a, i16 %b) {
entry:
  %result = add i16 %a, %b
  ret i16 %result
}

define i8 @test8(i8 %a, i8 %b) {
entry:
  %result = add i8 %a, %b
  ret i8 %result
}
)";

    std::string config = R"({
        "mba": {
            "enabled": true,
            "probability": 1.0
        }
    })";

    auto obfuscated = obfuscateIR(ir, config);
    ASSERT_FALSE(obfuscated.empty());

    // Should have all function definitions
    EXPECT_TRUE(irContains(obfuscated, "@test64"));
    EXPECT_TRUE(irContains(obfuscated, "@test16"));
    EXPECT_TRUE(irContains(obfuscated, "@test8"));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(IRIntegrationTest, EmptyFunction) {
    const char* ir = R"(
define void @empty() {
entry:
  ret void
}
)";

    auto obfuscated = obfuscateIR(ir);
    ASSERT_FALSE(obfuscated.empty());

    // Should preserve empty function
    EXPECT_TRUE(irContains(obfuscated, "define void @empty"));
    EXPECT_TRUE(irContains(obfuscated, "ret void"));
}

TEST_F(IRIntegrationTest, NoOperationsToTransform) {
    const char* ir = R"(
define i32 @passthrough(i32 %a) {
entry:
  ret i32 %a
}
)";

    auto obfuscated = obfuscateIR(ir);
    ASSERT_FALSE(obfuscated.empty());

    // Should be essentially unchanged
    EXPECT_TRUE(irContains(obfuscated, "ret i32"));
}

TEST_F(IRIntegrationTest, PreservesMetadata) {
    const char* ir = R"(
define i32 @test(i32 %a, i32 %b) !dbg !0 {
entry:
  %result = add i32 %a, %b, !dbg !1
  ret i32 %result
}

!llvm.dbg.cu = !{!2}
!0 = !DISubprogram(name: "test")
!1 = !DILocation(line: 1, column: 1)
!2 = !DICompileUnit()
)";

    auto obfuscated = obfuscateIR(ir);
    ASSERT_FALSE(obfuscated.empty());

    // Should preserve debug metadata
    EXPECT_TRUE(irContains(obfuscated, "!dbg") || irContains(obfuscated, "!llvm"));
}
