/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * constant_obf.hpp - Constant obfuscation pass
 *
 * Transforms constant values into equivalent expressions:
 *   - XOR: 42 -> (42 ^ key) ^ key
 *   - Split: 42 -> 27 + 15
 *   - Arithmetic: 42 -> (42 + offset) - offset
 *   - Multiply/Divide: 42 -> (42 * 7) / 7
 *   - Bit Split: 42 -> (32 | 8 | 2)
 */

#ifndef MORPHECT_CONSTANT_OBF_HPP
#define MORPHECT_CONSTANT_OBF_HPP

#include "data_base.hpp"
#include "../../core/transformation_base.hpp"

#include <regex>
#include <cmath>
#include <cstring>

namespace morphect {
namespace data {

/**
 * Constant Obfuscator - handles the actual obfuscation logic
 */
class ConstantObfuscator {
public:
    ConstantObfuscator() : logger_("ConstantObf") {}

    /**
     * Configure the obfuscator
     */
    void configure(const ConstantObfConfig& config) {
        config_ = config;
    }

    /**
     * Check if a constant should be obfuscated
     */
    bool shouldObfuscate(int64_t value) const {
        if (!config_.enabled) return false;

        // Check special values
        if (value == 0 && !config_.obfuscate_zero) return false;
        if ((value == 1 || value == -1) && !config_.obfuscate_one) return false;

        // Check range
        int64_t abs_val = value < 0 ? -value : value;
        if (abs_val < config_.min_value) return false;
        if (abs_val > config_.max_value) return false;

        // Probability check
        return GlobalRandom::decide(config_.probability);
    }

    /**
     * Obfuscate a constant using random strategy
     */
    ObfuscatedConstant obfuscate(int64_t value) const {
        if (config_.strategies.empty()) {
            return obfuscateXOR(value);
        }

        // Select random strategy
        size_t idx = GlobalRandom::nextSize(config_.strategies.size());
        return obfuscateWithStrategy(value, config_.strategies[idx]);
    }

    /**
     * Obfuscate with specific strategy
     */
    ObfuscatedConstant obfuscateWithStrategy(int64_t value,
                                             ConstantObfStrategy strategy) const {
        switch (strategy) {
            case ConstantObfStrategy::XOR:
                return obfuscateXOR(value);
            case ConstantObfStrategy::Split:
                return obfuscateSplit(value);
            case ConstantObfStrategy::Arithmetic:
                return obfuscateArithmetic(value);
            case ConstantObfStrategy::MultiplyDivide:
                return obfuscateMultiplyDivide(value);
            case ConstantObfStrategy::BitSplit:
                return obfuscateBitSplit(value);
            case ConstantObfStrategy::MBA:
                return obfuscateMBA(value);
            case ConstantObfStrategy::MultiSplit:
                return obfuscateMultiSplit(value);
            case ConstantObfStrategy::NestedXOR:
                return obfuscateNestedXOR(value);
            case ConstantObfStrategy::ShiftAdd:
                return obfuscateShiftAdd(value);
            default:
                return obfuscateXOR(value);
        }
    }

private:
    ConstantObfConfig config_;
    Logger logger_;

    /**
     * XOR obfuscation: c = (c ^ key) ^ key
     */
    ObfuscatedConstant obfuscateXOR(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::XOR;
        result.original = value;

        // Generate random key
        int64_t key = GlobalRandom::nextInt(1, 0xFFFF);
        int64_t encoded = value ^ key;

        result.parts = {encoded, key};
        result.expression = "(" + std::to_string(encoded) + " ^ " +
                           std::to_string(key) + ")";

        return result;
    }

    /**
     * Split obfuscation: c = c1 + c2
     */
    ObfuscatedConstant obfuscateSplit(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::Split;
        result.original = value;

        // Generate random split
        int64_t part1 = GlobalRandom::nextInt(-1000, 1000);
        int64_t part2 = value - part1;

        result.parts = {part1, part2};

        // Use subtraction sometimes for variety
        if (part2 < 0) {
            result.expression = "(" + std::to_string(part1) + " - " +
                               std::to_string(-part2) + ")";
        } else {
            result.expression = "(" + std::to_string(part1) + " + " +
                               std::to_string(part2) + ")";
        }

        return result;
    }

    /**
     * Arithmetic obfuscation: c = (c + offset) - offset
     */
    ObfuscatedConstant obfuscateArithmetic(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::Arithmetic;
        result.original = value;

        // Generate random offset
        int64_t offset = GlobalRandom::nextInt(100, 10000);
        int64_t encoded = value + offset;

        result.parts = {encoded, offset};
        result.expression = "(" + std::to_string(encoded) + " - " +
                           std::to_string(offset) + ")";

        return result;
    }

    /**
     * Multiply/Divide obfuscation: c = (c * factor) / factor
     *
     * Note: Only works for values divisible by factor, or we use
     * approximate division with correction.
     */
    ObfuscatedConstant obfuscateMultiplyDivide(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::MultiplyDivide;
        result.original = value;

        // Find a suitable factor
        std::vector<int64_t> factors = {2, 3, 4, 5, 7, 8, 11, 13};
        int64_t factor = factors[GlobalRandom::nextSize(factors.size())];

        int64_t multiplied = value * factor;

        result.parts = {multiplied, factor};
        result.expression = "(" + std::to_string(multiplied) + " / " +
                           std::to_string(factor) + ")";

        return result;
    }

    /**
     * Bit split obfuscation: c = (bit1 | bit2 | ...)
     *
     * Decomposes into individual bits that are OR'd together.
     */
    ObfuscatedConstant obfuscateBitSplit(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::BitSplit;
        result.original = value;

        // Handle negative numbers
        bool negative = value < 0;
        uint64_t abs_val = negative ? static_cast<uint64_t>(-value) : static_cast<uint64_t>(value);

        // Extract set bits
        std::vector<int64_t> bits;
        for (int i = 0; i < 64 && abs_val != 0; i++) {
            if (abs_val & 1) {
                bits.push_back(1LL << i);
            }
            abs_val >>= 1;
        }

        if (bits.empty()) {
            // Value is 0
            result.parts = {0};
            result.expression = "0";
            return result;
        }

        result.parts = bits;

        // Build expression
        std::ostringstream oss;
        oss << "(";
        for (size_t i = 0; i < bits.size(); i++) {
            if (i > 0) oss << " | ";
            oss << bits[i];
        }
        oss << ")";

        if (negative) {
            result.expression = "-" + oss.str();
        } else {
            result.expression = oss.str();
        }

        return result;
    }

    // ========================================================================
    // Enhanced Strategies (Phase 3.4)
    // ========================================================================

    /**
     * MBA-based obfuscation: Express constant using MBA identity
     *
     * Uses the identity: a + b = (a ^ b) + 2*(a & b)
     * So we find a, b such that a + b = value, then express it as MBA
     */
    ObfuscatedConstant obfuscateMBA(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::MBA;
        result.original = value;

        // Split value into two parts: a + b = value
        int64_t a = GlobalRandom::nextInt(-10000, 10000);
        int64_t b = value - a;

        // Now express a + b using MBA identity: (a ^ b) + 2*(a & b)
        // For the expression, we'll use actual computed values
        int64_t xor_part = a ^ b;
        int64_t and_part = a & b;

        result.parts = {a, b, xor_part, and_part};

        // Expression: (xor_part) + 2 * (and_part)
        // Which equals (a ^ b) + 2*(a & b) = a + b = value
        std::ostringstream oss;
        oss << "((" << xor_part << ") + 2 * (" << and_part << "))";
        result.expression = oss.str();

        return result;
    }

    /**
     * Multi-split obfuscation: c = c1 + c2 + c3 + ... + cn
     *
     * Splits the constant into multiple parts that sum to the value.
     */
    ObfuscatedConstant obfuscateMultiSplit(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::MultiSplit;
        result.original = value;

        // Determine number of parts (2 to max_multi_split_parts inclusive)
        // nextInt(min, max) returns [min, max] inclusive
        int num_parts = GlobalRandom::nextInt(2, config_.max_multi_split_parts);

        std::vector<int64_t> parts;
        int64_t remaining = value;

        // Generate random parts except the last one
        for (int i = 0; i < num_parts - 1; i++) {
            int64_t part = GlobalRandom::nextInt(-5000, 5000);
            parts.push_back(part);
            remaining -= part;
        }
        // Last part is the remainder
        parts.push_back(remaining);

        result.parts = parts;

        // Build expression
        std::ostringstream oss;
        oss << "(";
        for (size_t i = 0; i < parts.size(); i++) {
            if (i > 0) {
                if (parts[i] >= 0) {
                    oss << " + " << parts[i];
                } else {
                    oss << " - " << (-parts[i]);
                }
            } else {
                oss << parts[i];
            }
        }
        oss << ")";
        result.expression = oss.str();

        return result;
    }

    /**
     * Nested XOR obfuscation: c = ((c ^ k1) ^ k2) ^ k3 ...
     *
     * Each XOR can be reversed by XORing with the same key.
     * Final result: apply all keys in reverse order.
     */
    ObfuscatedConstant obfuscateNestedXOR(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::NestedXOR;
        result.original = value;

        // Determine depth (2 to max_nested_xor_depth inclusive)
        // nextInt(min, max) returns [min, max] inclusive
        int depth = GlobalRandom::nextInt(2, config_.max_nested_xor_depth);

        std::vector<int64_t> keys;
        int64_t encoded = value;

        // Generate keys and encode
        for (int i = 0; i < depth; i++) {
            int64_t key = GlobalRandom::nextInt(1, 0xFFFFFF);
            keys.push_back(key);
            encoded ^= key;
        }

        // Parts: encoded value followed by keys in reverse order
        result.parts.push_back(encoded);
        for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
            result.parts.push_back(*it);
        }

        // Build expression: ((encoded ^ key_n) ^ key_n-1) ^ ... ^ key_1
        std::ostringstream oss;
        oss << "(";
        for (int i = 0; i < depth; i++) {
            oss << "(";
        }
        oss << encoded;
        for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
            oss << " ^ " << *it << ")";
        }
        result.expression = oss.str();

        return result;
    }

    /**
     * Shift-add obfuscation: c = (a << n) + b
     *
     * Expresses constant as a shifted value plus an offset.
     * Works well for larger constants.
     */
    ObfuscatedConstant obfuscateShiftAdd(int64_t value) const {
        ObfuscatedConstant result;
        result.strategy = ConstantObfStrategy::ShiftAdd;
        result.original = value;

        // Choose shift amount (1 to 15 bits)
        int shift = GlobalRandom::nextInt(1, 16);
        int64_t divisor = 1LL << shift;

        // Calculate base and remainder
        int64_t base = value / divisor;
        int64_t remainder = value - (base << shift);

        result.parts = {base, static_cast<int64_t>(shift), remainder};

        // Build expression: (base << shift) + remainder
        std::ostringstream oss;
        oss << "((" << base << " << " << shift << ")";
        if (remainder >= 0) {
            oss << " + " << remainder;
        } else {
            oss << " - " << (-remainder);
        }
        oss << ")";
        result.expression = oss.str();

        return result;
    }

public:
    // ========================================================================
    // Floating Point Support (Phase 3.4)
    // ========================================================================

    /**
     * Check if a floating point constant should be obfuscated
     */
    bool shouldObfuscateFloat(double value) const {
        if (!config_.enabled || !config_.handle_floating_point) return false;

        // Skip special values
        if (std::isnan(value) || std::isinf(value)) return false;

        // Skip common small values
        double abs_val = std::fabs(value);
        if (abs_val < 0.001 && abs_val > 0.0) return false;  // Skip very small
        if (abs_val == 0.0 || abs_val == 1.0) return false;

        return GlobalRandom::decide(config_.probability);
    }

    /**
     * Obfuscate a floating point constant
     *
     * Strategy: Convert to integer bit representation, obfuscate, then
     * convert back to float using bitcast.
     */
    ObfuscatedFloat obfuscateFloat(double value) const {
        ObfuscatedFloat result;
        result.strategy = ConstantObfStrategy::XOR;  // Default
        result.original = value;

        // Get bit representation
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        result.int_bits = bits;

        // Obfuscate the bits using XOR
        int64_t key = GlobalRandom::nextInt(1, 0xFFFFFFFF);
        int64_t encoded = static_cast<int64_t>(bits) ^ key;

        result.parts = {encoded, key};

        // Expression for reconstructing: bitcast((encoded ^ key) to double)
        std::ostringstream oss;
        oss << "bitcast((" << encoded << " ^ " << key << ") to double)";
        result.expression = oss.str();

        return result;
    }

    /**
     * Obfuscate a single-precision float
     */
    ObfuscatedFloat obfuscateFloatSingle(float value) const {
        ObfuscatedFloat result;
        result.strategy = ConstantObfStrategy::XOR;
        result.original = static_cast<double>(value);

        // Get bit representation
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        result.int_bits = bits;

        // Obfuscate the bits using XOR
        int32_t key = GlobalRandom::nextInt(1, 0xFFFF);
        int32_t encoded = static_cast<int32_t>(bits) ^ key;

        result.parts = {encoded, key};

        // Expression for reconstructing
        std::ostringstream oss;
        oss << "bitcast((" << encoded << " ^ " << key << ") to float)";
        result.expression = oss.str();

        return result;
    }
};

/**
 * LLVM IR Constant Obfuscation Pass
 */
class LLVMConstantObfPass : public LLVMTransformationPass {
public:
    LLVMConstantObfPass() : logger_("ConstantObfPass") {}

    std::string getName() const override { return "ConstantObf"; }
    std::string getDescription() const override {
        return "Obfuscates constant values";
    }

    PassPriority getPriority() const override { return PassPriority::Data; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        const_config_.probability = config.probability;
        obfuscator_.configure(const_config_);
        return true;
    }

    void configure(const ConstantObfConfig& config) {
        const_config_ = config;
        obfuscator_.configure(config);
    }

    /**
     * Transform LLVM IR
     *
     * Finds constant values in instructions and replaces with expressions
     */
    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!const_config_.enabled) {
            return TransformResult::Skipped;
        }

        int transformations = 0;
        std::vector<std::string> new_lines;
        new_lines.reserve(lines.size() * 2);

        // Pattern for instructions with integer constants
        // e.g., %x = add i32 %y, 42
        std::regex const_pattern(
            R"((\s*)(%[\w.]+)\s*=\s*(\w+)\s+(\w+)\s+(%[\w.]+),\s*(-?\d+))"
        );

        // Pattern for constant as first operand
        std::regex const_pattern2(
            R"((\s*)(%[\w.]+)\s*=\s*(\w+)\s+(\w+)\s+(-?\d+),\s*(%[\w.]+))"
        );

        static int temp_counter = 0;

        for (const auto& line : lines) {
            bool transformed = false;
            std::smatch match;

            // Try pattern 1: op %var, constant
            if (std::regex_match(line, match, const_pattern)) {
                std::string indent = match[1];
                std::string dest = match[2];
                std::string op = match[3];
                std::string type = match[4];
                std::string var = match[5];
                int64_t constant = std::stoll(match[6]);

                if (obfuscator_.shouldObfuscate(constant)) {
                    auto obf = obfuscator_.obfuscate(constant);

                    // Generate instructions for the obfuscated constant
                    std::string temp = "%const_obf_" + std::to_string(temp_counter++);

                    // Generate the constant computation
                    auto const_lines = generateConstantComputation(
                        temp, type, obf, indent);

                    for (const auto& cl : const_lines) {
                        new_lines.push_back(cl);
                    }

                    // Replace original instruction
                    new_lines.push_back(indent + dest + " = " + op + " " +
                                       type + " " + var + ", " + temp);

                    transformed = true;
                    transformations++;
                    incrementStat("constants_obfuscated");
                }
            }

            // Try pattern 2: op constant, %var (less common)
            if (!transformed && std::regex_match(line, match, const_pattern2)) {
                std::string indent = match[1];
                std::string dest = match[2];
                std::string op = match[3];
                std::string type = match[4];
                int64_t constant = std::stoll(match[5]);
                std::string var = match[6];

                if (obfuscator_.shouldObfuscate(constant)) {
                    auto obf = obfuscator_.obfuscate(constant);

                    std::string temp = "%const_obf_" + std::to_string(temp_counter++);

                    auto const_lines = generateConstantComputation(
                        temp, type, obf, indent);

                    for (const auto& cl : const_lines) {
                        new_lines.push_back(cl);
                    }

                    new_lines.push_back(indent + dest + " = " + op + " " +
                                       type + " " + temp + ", " + var);

                    transformed = true;
                    transformations++;
                    incrementStat("constants_obfuscated");
                }
            }

            if (!transformed) {
                new_lines.push_back(line);
            }
        }

        lines = std::move(new_lines);
        incrementStat("total_transformations", transformations);

        return transformations > 0 ? TransformResult::Success : TransformResult::NotApplicable;
    }

private:
    Logger logger_;
    ConstantObfConfig const_config_;
    ConstantObfuscator obfuscator_;

    /**
     * Generate LLVM IR instructions to compute an obfuscated constant
     */
    std::vector<std::string> generateConstantComputation(
        const std::string& dest,
        const std::string& type,
        const ObfuscatedConstant& obf,
        const std::string& indent) const {

        std::vector<std::string> result;
        static int temp_counter = 0;

        switch (obf.strategy) {
            case ConstantObfStrategy::XOR: {
                // %t = xor type encoded, key
                result.push_back(indent + dest + " = xor " + type + " " +
                               std::to_string(obf.parts[0]) + ", " +
                               std::to_string(obf.parts[1]));
                break;
            }
            case ConstantObfStrategy::Split: {
                // %dest = add type part1, part2
                result.push_back(indent + dest + " = add " + type + " " +
                               std::to_string(obf.parts[0]) + ", " +
                               std::to_string(obf.parts[1]));
                break;
            }
            case ConstantObfStrategy::Arithmetic: {
                // %dest = sub type encoded, offset
                result.push_back(indent + dest + " = sub " + type + " " +
                               std::to_string(obf.parts[0]) + ", " +
                               std::to_string(obf.parts[1]));
                break;
            }
            case ConstantObfStrategy::MultiplyDivide: {
                // %dest = sdiv type multiplied, factor
                result.push_back(indent + dest + " = sdiv " + type + " " +
                               std::to_string(obf.parts[0]) + ", " +
                               std::to_string(obf.parts[1]));
                break;
            }
            case ConstantObfStrategy::BitSplit: {
                if (obf.parts.size() == 1) {
                    // Single bit or zero
                    result.push_back(indent + dest + " = add " + type + " " +
                                   std::to_string(obf.parts[0]) + ", 0");
                } else {
                    // OR all bits together
                    std::string current = std::to_string(obf.parts[0]);
                    for (size_t i = 1; i < obf.parts.size(); i++) {
                        std::string temp = "%bit_obf_" + std::to_string(temp_counter++);
                        result.push_back(indent + temp + " = or " + type + " " +
                                       current + ", " + std::to_string(obf.parts[i]));
                        current = temp;
                    }
                    // Copy final result to dest
                    result.push_back(indent + dest + " = add " + type + " " +
                                   current + ", 0");
                }
                break;
            }

            // ================================================================
            // Enhanced Strategies (Phase 3.4)
            // ================================================================

            case ConstantObfStrategy::MBA: {
                // MBA: (xor_part) + 2 * (and_part)
                // parts[2] = xor_part, parts[3] = and_part
                std::string t1 = "%mba_mul_" + std::to_string(temp_counter++);
                // %t1 = mul type 2, and_part
                result.push_back(indent + t1 + " = mul " + type + " 2, " +
                               std::to_string(obf.parts[3]));
                // %dest = add type xor_part, %t1
                result.push_back(indent + dest + " = add " + type + " " +
                               std::to_string(obf.parts[2]) + ", " + t1);
                break;
            }

            case ConstantObfStrategy::MultiSplit: {
                // Multi-split: c1 + c2 + c3 + ...
                if (obf.parts.empty()) {
                    result.push_back(indent + dest + " = add " + type + " 0, 0");
                } else if (obf.parts.size() == 1) {
                    result.push_back(indent + dest + " = add " + type + " " +
                                   std::to_string(obf.parts[0]) + ", 0");
                } else {
                    // Add all parts together
                    std::string current = std::to_string(obf.parts[0]);
                    for (size_t i = 1; i < obf.parts.size(); i++) {
                        std::string temp = "%msplit_" + std::to_string(temp_counter++);
                        result.push_back(indent + temp + " = add " + type + " " +
                                       current + ", " + std::to_string(obf.parts[i]));
                        current = temp;
                    }
                    // Copy final result to dest
                    result.push_back(indent + dest + " = add " + type + " " +
                                   current + ", 0");
                }
                break;
            }

            case ConstantObfStrategy::NestedXOR: {
                // Nested XOR: ((encoded ^ key_n) ^ key_n-1) ^ ... ^ key_1
                // parts[0] = encoded, parts[1..n] = keys in reverse order
                if (obf.parts.size() < 2) {
                    result.push_back(indent + dest + " = add " + type + " " +
                                   std::to_string(obf.parts[0]) + ", 0");
                } else {
                    std::string current = std::to_string(obf.parts[0]);
                    for (size_t i = 1; i < obf.parts.size(); i++) {
                        std::string temp = "%nxor_" + std::to_string(temp_counter++);
                        result.push_back(indent + temp + " = xor " + type + " " +
                                       current + ", " + std::to_string(obf.parts[i]));
                        current = temp;
                    }
                    // Copy final result to dest
                    result.push_back(indent + dest + " = add " + type + " " +
                                   current + ", 0");
                }
                break;
            }

            case ConstantObfStrategy::ShiftAdd: {
                // Shift-add: (base << shift) + remainder
                // parts[0] = base, parts[1] = shift, parts[2] = remainder
                std::string t1 = "%shift_" + std::to_string(temp_counter++);
                // %t1 = shl type base, shift
                result.push_back(indent + t1 + " = shl " + type + " " +
                               std::to_string(obf.parts[0]) + ", " +
                               std::to_string(obf.parts[1]));
                // %dest = add type %t1, remainder
                result.push_back(indent + dest + " = add " + type + " " +
                               t1 + ", " + std::to_string(obf.parts[2]));
                break;
            }
        }

        return result;
    }
};

/**
 * GIMPLE Constant Obfuscation Pass (interface)
 */
class GimpleConstantObfPass : public GimpleTransformationPass {
public:
    GimpleConstantObfPass() : logger_("GimpleConstantObf") {}

    std::string getName() const override { return "ConstantObf"; }
    std::string getDescription() const override {
        return "Obfuscates constant values";
    }

    PassPriority getPriority() const override { return PassPriority::Data; }

    void configure(const ConstantObfConfig& config) {
        config_ = config;
        obfuscator_.configure(config);
    }

    TransformResult transformGimple(void* func) override {
        // GIMPLE implementation is in the plugin
        (void)func;
        return TransformResult::NotApplicable;
    }

private:
    Logger logger_;
    ConstantObfConfig config_;
    ConstantObfuscator obfuscator_;
};

} // namespace data
} // namespace morphect

#endif // MORPHECT_CONSTANT_OBF_HPP
