/**
 * Morphect - Obfuscation Test Fixture
 *
 * Base class for all obfuscation tests. Provides utilities for:
 * - Compiling test programs with and without obfuscation
 * - Running compiled programs
 * - Comparing outputs
 */

#ifndef MORPHECT_OBFUSCATION_FIXTURE_HPP
#define MORPHECT_OBFUSCATION_FIXTURE_HPP

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <array>
#include <memory>

namespace morphect {
namespace test {

/**
 * Result of a command execution
 */
struct CommandResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;

    bool success() const { return exit_code == 0; }
};

/**
 * Base fixture for obfuscation tests
 */
class ObfuscationFixture : public ::testing::Test {
protected:
    // Paths
    std::filesystem::path test_dir_;
    std::filesystem::path build_dir_;
    std::filesystem::path plugin_path_;
    std::filesystem::path ir_obf_path_;

    // Compiler settings
    std::string compiler_ = "gcc";
    std::string cxx_compiler_ = "g++";
    std::vector<std::string> base_flags_ = {"-O2"};

    void SetUp() override {
        // Set up test directory
        test_dir_ = std::filesystem::temp_directory_path() / "morphect_test";
        std::filesystem::create_directories(test_dir_);

        // Find build directory (relative to test executable)
        build_dir_ = findBuildDir();
        plugin_path_ = build_dir_ / "lib" / "morphect_plugin.so";
        ir_obf_path_ = build_dir_ / "bin" / "morphect-ir";
    }

    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }

    /**
     * Write a test source file
     */
    std::filesystem::path writeSource(const std::string& filename,
                                       const std::string& content) {
        auto path = test_dir_ / filename;
        std::ofstream file(path);
        file << content;
        return path;
    }

    /**
     * Compile a C source file without obfuscation
     */
    CommandResult compileNormal(const std::filesystem::path& source,
                                 const std::filesystem::path& output,
                                 const std::vector<std::string>& extra_flags = {}) {
        std::vector<std::string> flags = base_flags_;
        flags.insert(flags.end(), extra_flags.begin(), extra_flags.end());

        std::string cmd = compiler_ + " " + source.string() + " -o " + output.string();
        for (const auto& flag : flags) {
            cmd += " " + flag;
        }

        return runCommand(cmd);
    }

    /**
     * Compile a C source file with GIMPLE plugin obfuscation
     */
    CommandResult compileWithGimple(const std::filesystem::path& source,
                                     const std::filesystem::path& output,
                                     const std::string& plugin_args = "",
                                     const std::vector<std::string>& extra_flags = {}) {
        std::vector<std::string> flags = base_flags_;
        flags.insert(flags.end(), extra_flags.begin(), extra_flags.end());

        std::string cmd = compiler_ + " " + source.string() + " -o " + output.string();
        cmd += " -fplugin=" + plugin_path_.string();
        if (!plugin_args.empty()) {
            cmd += " -fplugin-arg-morphect_plugin-" + plugin_args;
        }
        for (const auto& flag : flags) {
            cmd += " " + flag;
        }

        return runCommand(cmd);
    }

    /**
     * Compile via LLVM IR with obfuscation
     */
    CommandResult compileWithIR(const std::filesystem::path& source,
                                 const std::filesystem::path& output,
                                 const std::string& config_file = "",
                                 const std::vector<std::string>& extra_flags = {}) {
        // Step 1: Compile to LLVM IR
        auto ir_file = test_dir_ / "temp.ll";
        std::string cmd = "clang -S -emit-llvm " + source.string() + " -o " + ir_file.string();
        auto result = runCommand(cmd);
        if (!result.success()) return result;

        // Step 2: Obfuscate IR
        auto obf_ir = test_dir_ / "temp_obf.ll";
        cmd = ir_obf_path_.string() + " " + ir_file.string() + " -o " + obf_ir.string();
        if (!config_file.empty()) {
            cmd += " --config " + config_file;
        }
        result = runCommand(cmd);
        if (!result.success()) return result;

        // Step 3: Compile obfuscated IR
        cmd = "clang " + obf_ir.string() + " -o " + output.string();
        for (const auto& flag : extra_flags) {
            cmd += " " + flag;
        }
        return runCommand(cmd);
    }

    /**
     * Run an executable and capture output
     */
    CommandResult runExecutable(const std::filesystem::path& exe,
                                 const std::vector<std::string>& args = {},
                                 const std::string& stdin_input = "") {
        std::string cmd = exe.string();
        for (const auto& arg : args) {
            cmd += " " + arg;
        }

        if (!stdin_input.empty()) {
            // Write input to temp file and redirect
            auto input_file = test_dir_ / "input.txt";
            std::ofstream f(input_file);
            f << stdin_input;
            cmd += " < " + input_file.string();
        }

        return runCommand(cmd);
    }

    /**
     * Compare two executables - they should produce same output
     */
    void compareExecutables(const std::filesystem::path& exe1,
                            const std::filesystem::path& exe2,
                            const std::vector<std::string>& args = {},
                            const std::string& stdin_input = "") {
        auto result1 = runExecutable(exe1, args, stdin_input);
        auto result2 = runExecutable(exe2, args, stdin_input);

        EXPECT_EQ(result1.exit_code, result2.exit_code)
            << "Exit codes differ: " << result1.exit_code << " vs " << result2.exit_code;
        EXPECT_EQ(result1.stdout_output, result2.stdout_output)
            << "Stdout differs";
    }

    /**
     * Assert that obfuscation preserves program semantics
     */
    void assertSemanticEquivalence(const std::string& source_code,
                                    const std::vector<std::string>& test_args = {}) {
        auto source = writeSource("test.c", source_code);
        auto normal_exe = test_dir_ / "test_normal";
        auto obf_exe = test_dir_ / "test_obf";

        // Compile both versions
        auto normal_result = compileNormal(source, normal_exe);
        ASSERT_TRUE(normal_result.success()) << "Normal compilation failed: "
            << normal_result.stderr_output;

        auto obf_result = compileWithGimple(source, obf_exe);
        ASSERT_TRUE(obf_result.success()) << "Obfuscated compilation failed: "
            << obf_result.stderr_output;

        // Compare outputs
        compareExecutables(normal_exe, obf_exe, test_args);
    }

protected:
    /**
     * Run a shell command and capture output
     */
    CommandResult runCommand(const std::string& cmd) {
        CommandResult result;

        // Create temp files for output
        auto stdout_file = test_dir_ / "stdout.txt";
        auto stderr_file = test_dir_ / "stderr.txt";

        std::string full_cmd = cmd + " > " + stdout_file.string() +
                               " 2> " + stderr_file.string();

        result.exit_code = std::system(full_cmd.c_str());
        result.exit_code = WEXITSTATUS(result.exit_code);

        // Read outputs
        std::ifstream stdout_stream(stdout_file);
        std::stringstream stdout_buf;
        stdout_buf << stdout_stream.rdbuf();
        result.stdout_output = stdout_buf.str();

        std::ifstream stderr_stream(stderr_file);
        std::stringstream stderr_buf;
        stderr_buf << stderr_stream.rdbuf();
        result.stderr_output = stderr_buf.str();

        return result;
    }

    /**
     * Find the build directory
     */
    std::filesystem::path findBuildDir() {
        // Check common locations
        std::vector<std::filesystem::path> candidates = {
            ".",
            "..",
            "../build",
            "../../build",
            std::filesystem::current_path() / "build"
        };

        for (const auto& dir : candidates) {
            auto plugin = dir / "lib" / "morphect_plugin.so";
            if (std::filesystem::exists(plugin)) {
                return std::filesystem::canonical(dir);
            }
        }

        // Default to parent of executable
        return std::filesystem::current_path();
    }
};

/**
 * Fixture for LLVM IR tests
 */
class LLVMIRFixture : public ObfuscationFixture {
protected:
    /**
     * Apply IR obfuscation to a string and return result
     */
    std::string obfuscateIR(const std::string& ir_content,
                            const std::string& config = "") {
        auto input_file = writeSource("input.ll", ir_content);
        auto output_file = test_dir_ / "output.ll";

        std::string cmd = ir_obf_path_.string() + " " + input_file.string() +
                          " -o " + output_file.string();
        if (!config.empty()) {
            auto config_file = writeSource("config.json", config);
            cmd += " --config " + config_file.string();
        }

        auto result = runCommand(cmd);
        if (!result.success()) {
            return "";
        }

        std::ifstream file(output_file);
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }

    /**
     * Check if IR contains a pattern
     */
    bool irContains(const std::string& ir, const std::string& pattern) {
        return ir.find(pattern) != std::string::npos;
    }

    /**
     * Count occurrences of a pattern in IR
     */
    int countInIR(const std::string& ir, const std::string& pattern) {
        int count = 0;
        size_t pos = 0;
        while ((pos = ir.find(pattern, pos)) != std::string::npos) {
            count++;
            pos += pattern.length();
        }
        return count;
    }
};

/**
 * Fixture for Assembly tests
 */
class AssemblyFixture : public ObfuscationFixture {
protected:
    std::filesystem::path asm_obf_path_;

    void SetUp() override {
        ObfuscationFixture::SetUp();
        asm_obf_path_ = build_dir_ / "bin" / "morphect-asm";
    }

    /**
     * Apply assembly obfuscation
     */
    std::string obfuscateAsm(const std::string& asm_content,
                             const std::string& config = "") {
        auto input_file = writeSource("input.s", asm_content);
        auto output_file = test_dir_ / "output.s";

        std::string cmd = asm_obf_path_.string() + " " + input_file.string() +
                          " -o " + output_file.string();
        if (!config.empty()) {
            auto config_file = writeSource("config.json", config);
            cmd += " --config " + config_file.string();
        }

        auto result = runCommand(cmd);
        if (!result.success()) {
            return "";
        }

        std::ifstream file(output_file);
        std::stringstream buf;
        buf << file.rdbuf();
        return buf.str();
    }
};

} // namespace test
} // namespace morphect

#endif // MORPHECT_OBFUSCATION_FIXTURE_HPP
