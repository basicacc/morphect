/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * dead_code_base.hpp - Base definitions for dead code generation
 *
 * Dead code obfuscation includes:
 *   - Realistic dead arithmetic (looks like real computation)
 *   - Dead memory accesses (stack, globals, arrays)
 *   - Dead function calls (to nop/empty functions)
 *   - MBA-obfuscated dead code (apply MBA to generated code)
 *
 * Key principle: Dead code should be indistinguishable from real code
 * to both static and dynamic analysis tools.
 */

#ifndef MORPHECT_DEAD_CODE_BASE_HPP
#define MORPHECT_DEAD_CODE_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_set>

namespace morphect {
namespace deadcode {

/**
 * Types of dead code that can be generated
 */
enum class DeadCodeType {
    Arithmetic,     // Dead arithmetic operations
    Memory,         // Dead memory accesses (load/store)
    Call,           // Dead function calls
    ControlFlow,    // Dead branches (using opaque predicates)
    Mixed           // Combination of above
};

/**
 * Arithmetic operation types for dead code
 */
enum class DeadArithOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
    Shl,
    Shr,
    // Floating point
    FAdd,
    FSub,
    FMul,
    FDiv
};

/**
 * Memory operation types for dead code
 */
enum class DeadMemOp {
    Load,           // Load from memory
    Store,          // Store to memory
    Alloca,         // Stack allocation
    GEP             // Getelementptr (array/struct access)
};

/**
 * Configuration for dead code generation
 */
struct DeadCodeConfig {
    bool enabled = true;
    double probability = 0.5;      // Probability to insert at each opportunity

    // Generation settings
    int min_ops_per_block = 2;     // Minimum dead ops per insertion point
    int max_ops_per_block = 6;     // Maximum dead ops per insertion point
    int min_blocks = 1;            // Minimum dead code blocks per function
    int max_blocks = 4;            // Maximum dead code blocks per function

    // Type probabilities
    double arithmetic_probability = 0.4;
    double memory_probability = 0.3;
    double call_probability = 0.2;
    double control_flow_probability = 0.1;

    // Features
    bool use_real_variables = true;      // Use existing variable names
    bool apply_mba = true;               // Apply MBA to dead arithmetic
    bool create_dependencies = true;     // Create fake data dependencies
    bool use_realistic_patterns = true;  // Generate common code patterns

    // Control
    std::vector<std::string> exclude_functions;  // Don't add dead code here
    int seed = 0;                                // RNG seed for reproducibility
};

/**
 * Information about a variable that can be used in dead code
 */
struct VariableInfo {
    std::string name;
    std::string type;
    bool is_local = true;
    bool is_pointer = false;
    bool is_array = false;
    int bit_width = 32;

    VariableInfo() = default;
    VariableInfo(const std::string& n, const std::string& t)
        : name(n), type(t) {}
};

/**
 * Generated dead code block
 */
struct DeadCodeBlock {
    std::vector<std::string> code;                    // The generated code lines
    std::vector<std::string> vars_created;            // Temporary vars created
    std::vector<std::string> vars_used;               // Existing vars used
    std::vector<std::string> nop_functions_created;   // Nop functions needed
    DeadCodeType type = DeadCodeType::Mixed;
    bool needs_guard = true;                          // Needs opaque predicate guard?
    int ops_inserted = 0;                             // Number of ops generated
    int calls_inserted = 0;                           // Number of calls generated
    int memory_ops_inserted = 0;                      // Number of memory ops generated

    /**
     * Get all code lines as single string
     */
    std::string toString() const {
        std::string result;
        for (const auto& line : code) {
            result += line + "\n";
        }
        return result;
    }
};

/**
 * Result of dead code insertion
 */
struct DeadCodeResult {
    bool success = false;
    std::vector<std::string> transformed_code;
    int blocks_inserted = 0;
    int ops_inserted = 0;
    int calls_inserted = 0;
    int memory_ops_inserted = 0;
    std::vector<std::string> nop_functions_created;
};

// ============================================================================
// Dead Code Generators
// ============================================================================

/**
 * Base class for dead code generators
 */
class DeadCodeGenerator {
public:
    virtual ~DeadCodeGenerator() = default;

    /**
     * Get the name of this generator
     */
    virtual std::string getName() const = 0;

    /**
     * Generate a block of dead code
     *
     * @param available_vars Variables that can be used
     * @param config Generation configuration
     * @return Generated dead code block
     */
    virtual DeadCodeBlock generate(
        const std::vector<VariableInfo>& available_vars,
        const DeadCodeConfig& config) = 0;

protected:
    static int temp_counter_;
    static int block_counter_;

    /**
     * Generate a unique temporary variable name
     */
    std::string nextTemp(const std::string& prefix = "_dead") {
        return "%" + prefix + std::to_string(temp_counter_++);
    }

    /**
     * Generate a unique label
     */
    std::string nextLabel(const std::string& prefix = "dead_block") {
        return prefix + std::to_string(block_counter_++);
    }

    /**
     * Select a random variable of appropriate type
     */
    const VariableInfo* selectVariable(
        const std::vector<VariableInfo>& vars,
        const std::string& required_type = "") const {

        std::vector<const VariableInfo*> candidates;
        for (const auto& v : vars) {
            if (required_type.empty() || v.type == required_type) {
                candidates.push_back(&v);
            }
        }
        if (candidates.empty()) return nullptr;
        // nextInt is inclusive, so use size-1
        return candidates[GlobalRandom::nextInt(0, static_cast<int>(candidates.size()) - 1)];
    }

    /**
     * Generate a random constant of given type
     */
    std::string randomConstant(const std::string& type) {
        if (type.find("i32") != std::string::npos) {
            return std::to_string(GlobalRandom::nextInt(-10000, 10000));
        } else if (type.find("i64") != std::string::npos) {
            return std::to_string(GlobalRandom::nextInt(-10000, 10000));
        } else if (type.find("i8") != std::string::npos) {
            return std::to_string(GlobalRandom::nextInt(0, 255));
        } else if (type.find("i16") != std::string::npos) {
            return std::to_string(GlobalRandom::nextInt(-1000, 1000));
        } else if (type.find("float") != std::string::npos) {
            return std::to_string(GlobalRandom::nextDouble() * 100.0);
        } else if (type.find("double") != std::string::npos) {
            return std::to_string(GlobalRandom::nextDouble() * 100.0);
        }
        return "0";
    }
};

/**
 * Generates realistic dead arithmetic operations
 */
class DeadArithmeticGenerator : public DeadCodeGenerator {
public:
    std::string getName() const override { return "DeadArithmetic"; }

    DeadCodeBlock generate(
        const std::vector<VariableInfo>& available_vars,
        const DeadCodeConfig& config) override {

        DeadCodeBlock block;
        block.type = DeadCodeType::Arithmetic;
        block.needs_guard = true;

        int num_ops = GlobalRandom::nextInt(config.min_ops_per_block,
                                            config.max_ops_per_block + 1);

        // Generate chain of operations
        std::string current_var = nextTemp("_arith_init");
        std::string type = "i32";

        // Initialize the first variable with a constant or use a real variable
        bool initialized = false;
        if (config.use_real_variables && !available_vars.empty()) {
            const auto* var = selectVariable(available_vars, "i32");
            if (var) {
                block.code.push_back("  " + current_var + " = add i32 " +
                    var->name + ", 0  ; use existing var");
                block.vars_used.push_back(var->name);
                block.vars_created.push_back(current_var);
                initialized = true;
            }
        }

        // Always initialize with a constant if not using a real variable
        if (!initialized) {
            block.code.push_back("  " + current_var + " = add i32 0, " +
                randomConstant(type) + "  ; initialize");
            block.vars_created.push_back(current_var);
        }

        for (int i = 0; i < num_ops; i++) {
            std::string new_var = nextTemp("_arith");
            std::string op_code = generateArithOp(current_var, new_var, type);
            block.code.push_back(op_code);
            block.vars_created.push_back(new_var);
            current_var = new_var;
        }

        block.ops_inserted = num_ops;
        return block;
    }

private:
    std::string generateArithOp(const std::string& input,
                                const std::string& output,
                                const std::string& type) {
        static const std::vector<std::string> ops = {
            "add", "sub", "mul", "and", "or", "xor", "shl", "lshr", "ashr"
        };

        // nextInt(min, max) is INCLUSIVE, so use size-1
        std::string op = ops[GlobalRandom::nextInt(0, static_cast<int>(ops.size()) - 1)];
        std::string constant = randomConstant(type);

        // Avoid division by zero
        if (op == "mul" || op == "add" || op == "sub") {
            return "  " + output + " = " + op + " " + type + " " +
                   input + ", " + constant;
        } else if (op == "shl" || op == "lshr" || op == "ashr") {
            // Shift amount should be reasonable
            int shift = GlobalRandom::nextInt(1, 8);
            return "  " + output + " = " + op + " " + type + " " +
                   input + ", " + std::to_string(shift);
        } else {
            return "  " + output + " = " + op + " " + type + " " +
                   input + ", " + constant;
        }
    }
};

/**
 * Generates dead memory operations
 */
class DeadMemoryGenerator : public DeadCodeGenerator {
public:
    std::string getName() const override { return "DeadMemory"; }

    DeadCodeBlock generate(
        const std::vector<VariableInfo>& available_vars,
        const DeadCodeConfig& config) override {

        DeadCodeBlock block;
        block.type = DeadCodeType::Memory;
        block.needs_guard = true;

        int num_ops = GlobalRandom::nextInt(config.min_ops_per_block,
                                            config.max_ops_per_block + 1);

        // Allocate some stack space first
        std::string alloca_var = nextTemp("_dead_mem");
        block.code.push_back("  " + alloca_var + " = alloca i32, align 4");
        block.vars_created.push_back(alloca_var);

        // Maybe create an array
        std::string array_var;
        if (GlobalRandom::nextDouble() < 0.5) {
            array_var = nextTemp("_dead_arr");
            int size = GlobalRandom::nextInt(4, 16);
            block.code.push_back("  " + array_var + " = alloca [" +
                std::to_string(size) + " x i32], align 4");
            block.vars_created.push_back(array_var);
        }

        for (int i = 0; i < num_ops; i++) {
            if (GlobalRandom::nextDouble() < 0.5) {
                // Store operation
                std::string val_var = nextTemp();
                block.code.push_back("  " + val_var + " = add i32 0, " +
                    randomConstant("i32"));
                block.code.push_back("  store i32 " + val_var + ", i32* " +
                    alloca_var + ", align 4");
                block.vars_created.push_back(val_var);
            } else {
                // Load operation
                std::string load_var = nextTemp("_dead_load");
                block.code.push_back("  " + load_var + " = load i32, i32* " +
                    alloca_var + ", align 4");
                block.vars_created.push_back(load_var);
            }
        }

        block.memory_ops_inserted = num_ops;
        return block;
    }
};

/**
 * Generates dead function calls
 */
class DeadCallGenerator : public DeadCodeGenerator {
public:
    std::string getName() const override { return "DeadCall"; }

    DeadCodeBlock generate(
        const std::vector<VariableInfo>& available_vars,
        const DeadCodeConfig& config) override {

        DeadCodeBlock block;
        block.type = DeadCodeType::Call;
        block.needs_guard = true;

        int num_calls = GlobalRandom::nextInt(1, 3);

        for (int i = 0; i < num_calls; i++) {
            std::string nop_func = generateNopFunctionName();
            block.nop_functions_created.push_back(nop_func);

            // Generate call with dummy arguments
            std::string call_code = "  call void @" + nop_func + "(";
            int num_args = GlobalRandom::nextInt(0, 3);
            for (int j = 0; j < num_args; j++) {
                if (j > 0) call_code += ", ";
                call_code += "i32 " + randomConstant("i32");
            }
            call_code += ")";
            block.code.push_back(call_code);
        }

        block.calls_inserted = num_calls;
        return block;
    }

    /**
     * Generate nop function definition
     */
    static std::vector<std::string> generateNopFunction(
        const std::string& name, int num_args) {

        std::vector<std::string> def;
        std::string params;
        for (int i = 0; i < num_args; i++) {
            if (i > 0) params += ", ";
            params += "i32 %_unused" + std::to_string(i);
        }

        def.push_back("; Dead code nop function");
        def.push_back("define internal void @" + name + "(" + params + ") {");
        def.push_back("entry:");
        def.push_back("  ret void");
        def.push_back("}");

        return def;
    }

private:
    std::string generateNopFunctionName() {
        static const std::vector<std::string> prefixes = {
            "_validate", "_check", "_verify", "_process",
            "_update", "_sync", "_cache", "_compute"
        };
        // nextInt(min, max) is INCLUSIVE, so use size-1
        std::string prefix = prefixes[GlobalRandom::nextInt(0,
            static_cast<int>(prefixes.size()) - 1)];
        return prefix + "_" + std::to_string(GlobalRandom::nextInt(1000, 9999));
    }
};

/**
 * Generates MBA-obfuscated dead code
 */
class MBADeadCodeGenerator : public DeadCodeGenerator {
public:
    std::string getName() const override { return "MBADeadCode"; }

    DeadCodeBlock generate(
        const std::vector<VariableInfo>& available_vars,
        const DeadCodeConfig& config) override {

        DeadCodeBlock block;
        block.type = DeadCodeType::Arithmetic;
        block.needs_guard = true;

        // Generate a simple addition, then expand to MBA
        // a + b = (a ^ b) + 2 * (a & b)
        std::string a = nextTemp("_mba_a");
        std::string b = nextTemp("_mba_b");
        std::string xor_ab = nextTemp("_mba_xor");
        std::string and_ab = nextTemp("_mba_and");
        std::string mul_2 = nextTemp("_mba_mul");
        std::string result = nextTemp("_mba_res");

        // Initialize with random or existing values
        std::string a_init, b_init;
        if (config.use_real_variables && !available_vars.empty()) {
            const auto* var = selectVariable(available_vars, "i32");
            if (var) {
                a_init = var->name;
                block.vars_used.push_back(var->name);
            } else {
                a_init = randomConstant("i32");
            }
        } else {
            a_init = randomConstant("i32");
        }
        b_init = randomConstant("i32");

        block.code.push_back("  ; MBA-obfuscated dead code: (a ^ b) + 2*(a & b)");
        block.code.push_back("  " + a + " = add i32 " + a_init + ", 0");
        block.code.push_back("  " + b + " = add i32 " + b_init + ", 0");
        block.code.push_back("  " + xor_ab + " = xor i32 " + a + ", " + b);
        block.code.push_back("  " + and_ab + " = and i32 " + a + ", " + b);
        block.code.push_back("  " + mul_2 + " = shl i32 " + and_ab + ", 1");
        block.code.push_back("  " + result + " = add i32 " + xor_ab + ", " + mul_2);

        block.vars_created = {a, b, xor_ab, and_ab, mul_2, result};
        block.ops_inserted = 6;

        return block;
    }
};

/**
 * Opaque predicate generator for guarding dead code
 */
class OpaquePredicateGen {
public:
    /**
     * Generate an always-false predicate
     * The dead code guarded by this will never execute
     */
    static std::pair<std::string, std::vector<std::string>>
    generateAlwaysFalse(const std::string& prefix) {
        int choice = GlobalRandom::nextInt(0, 3);  // 4 cases: 0, 1, 2, 3
        std::vector<std::string> setup;
        std::string condition;

        switch (choice) {
            case 0: {
                // x * x < 0 (always false for integers)
                std::string v = "%" + prefix + "_opq_v";
                std::string sq = "%" + prefix + "_opq_sq";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(1, 1000)));
                setup.push_back("  " + sq + " = mul i32 " + v + ", " + v);
                condition = "icmp slt i32 " + sq + ", 0";
                break;
            }
            case 1: {
                // (x & 1) == 2 (always false)
                std::string v = "%" + prefix + "_opq_v";
                std::string a = "%" + prefix + "_opq_a";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup.push_back("  " + a + " = and i32 " + v + ", 1");
                condition = "icmp eq i32 " + a + ", 2";
                break;
            }
            case 2: {
                // x == x + 1 (always false)
                std::string v = "%" + prefix + "_opq_v";
                std::string p1 = "%" + prefix + "_opq_p1";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup.push_back("  " + p1 + " = add i32 " + v + ", 1");
                condition = "icmp eq i32 " + v + ", " + p1;
                break;
            }
            case 3: {
                // (x | x) != x (always false)
                std::string v = "%" + prefix + "_opq_v";
                std::string o = "%" + prefix + "_opq_o";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup.push_back("  " + o + " = or i32 " + v + ", " + v);
                condition = "icmp ne i32 " + o + ", " + v;
                break;
            }
        }

        return {condition, setup};
    }

    /**
     * Generate an always-true predicate
     * Can be used for other purposes (real code always runs)
     */
    static std::pair<std::string, std::vector<std::string>>
    generateAlwaysTrue(const std::string& prefix) {
        int choice = GlobalRandom::nextInt(0, 3);  // 4 cases: 0, 1, 2, 3
        std::vector<std::string> setup;
        std::string condition;

        switch (choice) {
            case 0: {
                // x * x >= 0 (always true for integers)
                std::string v = "%" + prefix + "_opq_v";
                std::string sq = "%" + prefix + "_opq_sq";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(1, 1000)));
                setup.push_back("  " + sq + " = mul i32 " + v + ", " + v);
                condition = "icmp sge i32 " + sq + ", 0";
                break;
            }
            case 1: {
                // (x & 1) < 2 (always true)
                std::string v = "%" + prefix + "_opq_v";
                std::string a = "%" + prefix + "_opq_a";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup.push_back("  " + a + " = and i32 " + v + ", 1");
                condition = "icmp slt i32 " + a + ", 2";
                break;
            }
            case 2: {
                // x != x + 1 (always true)
                std::string v = "%" + prefix + "_opq_v";
                std::string p1 = "%" + prefix + "_opq_p1";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup.push_back("  " + p1 + " = add i32 " + v + ", 1");
                condition = "icmp ne i32 " + v + ", " + p1;
                break;
            }
            case 3: {
                // (x | x) == x (always true)
                std::string v = "%" + prefix + "_opq_v";
                std::string o = "%" + prefix + "_opq_o";
                setup.push_back("  " + v + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup.push_back("  " + o + " = or i32 " + v + ", " + v);
                condition = "icmp eq i32 " + o + ", " + v;
                break;
            }
        }

        return {condition, setup};
    }
};

// Initialize static counters
inline int DeadCodeGenerator::temp_counter_ = 0;
inline int DeadCodeGenerator::block_counter_ = 0;

/**
 * Abstract base for dead code transformations
 */
class DeadCodeTransformation {
public:
    virtual ~DeadCodeTransformation() = default;

    /**
     * Get the name of this transformation
     */
    virtual std::string getName() const = 0;

    /**
     * Transform code by inserting dead code
     *
     * @param lines The code lines to transform
     * @param config Configuration for dead code generation
     * @return Result of transformation
     */
    virtual DeadCodeResult transform(
        const std::vector<std::string>& lines,
        const DeadCodeConfig& config) = 0;

protected:
    /**
     * Check if a function should be excluded
     */
    bool shouldExclude(const std::string& func_name,
                       const DeadCodeConfig& config) const {
        for (const auto& pattern : config.exclude_functions) {
            if (func_name.find(pattern) != std::string::npos) {
                return true;
            }
        }
        // Always exclude our own generated functions
        if (func_name.find("_dead") != std::string::npos ||
            func_name.find("_validate_") != std::string::npos ||
            func_name.find("_check_") != std::string::npos ||
            func_name.find("_verify_") != std::string::npos) {
            return true;
        }
        return false;
    }
};

} // namespace deadcode
} // namespace morphect

#endif // MORPHECT_DEAD_CODE_BASE_HPP
