/**
 * Morphect - Indirect Call Obfuscation for LLVM IR
 *
 * Transforms direct function calls into indirect calls via function pointer tables.
 *
 * Original:
 *   %result = call i32 @target_func(i32 %arg)
 *
 * Transformed:
 *   ; Load function pointer from table
 *   %ptr_raw = load i8*, i8** getelementptr ([N x i8*], [N x i8*]* @_func_table, i64 0, i64 IDX)
 *   ; Decode address (XOR with key)
 *   %ptr_int = ptrtoint i8* %ptr_raw to i64
 *   %decoded_int = xor i64 %ptr_int, KEY
 *   %ptr = inttoptr i64 %decoded_int to i32 (i32)*
 *   ; Call through pointer
 *   %result = call i32 %ptr(i32 %arg)
 */

#ifndef MORPHECT_INDIRECT_CALL_HPP
#define MORPHECT_INDIRECT_CALL_HPP

#include "indirect_call_base.hpp"

#include <regex>
#include <sstream>
#include <algorithm>

namespace morphect {
namespace control_flow {

/**
 * LLVM IR Call Site Analyzer
 */
class LLVMCallSiteAnalyzer : public CallSiteAnalyzer {
public:
    std::vector<CallSiteInfo> findCalls(
        const std::vector<std::string>& lines) override {

        std::vector<CallSiteInfo> calls;
        int call_id = 0;
        std::string current_function;

        // Pattern for call instructions
        // Matches: %result = call type @func(args)
        //      or: call void @func(args)
        std::regex call_with_result(
            R"(^\s*(%[\w.]+)\s*=\s*(?:tail\s+|musttail\s+)?call\s+(\S+)\s+@(\w+)\s*\(([^)]*)\)(.*)$)");
        std::regex call_void(
            R"(^\s*(?:tail\s+|musttail\s+)?call\s+void\s+@(\w+)\s*\(([^)]*)\)(.*)$)");
        std::regex call_with_type(
            R"(^\s*(%[\w.]+)\s*=\s*(?:tail\s+|musttail\s+)?call\s+([^@]+)@(\w+)\s*\(([^)]*)\)(.*)$)");

        // Function definition pattern
        std::regex func_def(R"(^\s*define\s+.*@(\w+)\s*\()");

        for (size_t i = 0; i < lines.size(); i++) {
            const std::string& line = lines[i];
            std::smatch match;

            // Track current function
            if (std::regex_search(line, match, func_def)) {
                current_function = match[1].str();
                continue;
            }

            // Skip intrinsics and special calls early
            if (line.find("@llvm.") != std::string::npos) {
                continue;
            }

            // Call with result
            if (std::regex_search(line, match, call_with_result)) {
                CallSiteInfo info;
                info.id = call_id++;
                info.caller_function = current_function;
                info.result_var = match[1].str();
                info.callee_type = match[2].str();
                info.callee_function = match[3].str();
                info.original_instruction = line;
                info.line_number = static_cast<int>(i);
                info.call_attributes = match[5].str();

                // Parse arguments
                parseArguments(match[4].str(), info.arguments);

                // Check for tail call
                info.is_tail_call = (line.find("tail call") != std::string::npos);
                info.is_must_tail = (line.find("musttail") != std::string::npos);

                calls.push_back(info);
                continue;
            }

            // Call with complex type (includes function type before @)
            if (std::regex_search(line, match, call_with_type)) {
                CallSiteInfo info;
                info.id = call_id++;
                info.caller_function = current_function;
                info.result_var = match[1].str();
                info.callee_type = trimWhitespace(match[2].str());
                info.callee_function = match[3].str();
                info.original_instruction = line;
                info.line_number = static_cast<int>(i);
                info.call_attributes = match[5].str();

                parseArguments(match[4].str(), info.arguments);
                info.is_tail_call = (line.find("tail call") != std::string::npos);

                calls.push_back(info);
                continue;
            }

            // Void call
            if (std::regex_search(line, match, call_void)) {
                CallSiteInfo info;
                info.id = call_id++;
                info.caller_function = current_function;
                info.callee_type = "void";
                info.callee_function = match[1].str();
                info.original_instruction = line;
                info.line_number = static_cast<int>(i);
                info.call_attributes = match[3].str();

                parseArguments(match[2].str(), info.arguments);
                info.is_tail_call = (line.find("tail call") != std::string::npos);

                calls.push_back(info);
                continue;
            }
        }

        return calls;
    }

    std::unordered_map<std::string, FunctionInfo> extractFunctions(
        const std::vector<std::string>& lines) override {

        std::unordered_map<std::string, FunctionInfo> functions;

        // Pattern for function definitions and declarations
        std::regex func_def(
            R"(^\s*(define|declare)\s+(\S+)\s+@(\w+)\s*\(([^)]*)\))");

        for (const auto& line : lines) {
            std::smatch match;
            if (std::regex_search(line, match, func_def)) {
                FunctionInfo info;
                info.name = match[3].str();
                info.return_type = match[2].str();
                info.is_declaration = (match[1].str() == "declare");
                info.is_external = info.is_declaration;

                // Parse parameters
                std::string params = match[4].str();
                if (params.find("...") != std::string::npos) {
                    info.is_vararg = true;
                }

                // Build full signature
                info.full_signature = info.return_type + " (" + params + ")";

                // Parse parameter types
                parseParamTypes(params, info.param_types);

                functions[info.name] = info;
            }
        }

        return functions;
    }

private:
    void parseArguments(const std::string& args_str,
                       std::vector<std::string>& args) {
        if (args_str.empty()) return;

        // Split by comma, but be careful with nested types
        std::string current;
        int depth = 0;

        for (char c : args_str) {
            if (c == '(' || c == '[' || c == '{' || c == '<') {
                depth++;
                current += c;
            } else if (c == ')' || c == ']' || c == '}' || c == '>') {
                depth--;
                current += c;
            } else if (c == ',' && depth == 0) {
                args.push_back(trimWhitespace(current));
                current.clear();
            } else {
                current += c;
            }
        }

        if (!current.empty()) {
            args.push_back(trimWhitespace(current));
        }
    }

    void parseParamTypes(const std::string& params_str,
                        std::vector<std::string>& types) {
        std::vector<std::string> args;
        parseArguments(params_str, args);

        for (const auto& arg : args) {
            // Extract type (everything before the last space + variable name)
            size_t last_space = arg.rfind(' ');
            if (last_space != std::string::npos) {
                std::string type = arg.substr(0, last_space);
                types.push_back(trimWhitespace(type));
            } else if (arg != "...") {
                types.push_back(arg);
            }
        }
    }

    static std::string trimWhitespace(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
};

/**
 * LLVM IR Indirect Call Transformation
 */
class LLVMIndirectCallTransformation : public IndirectCallTransformation {
public:
    std::string getName() const override { return "LLVM_IndirectCall"; }

    IndirectCallResult transform(
        const std::vector<std::string>& lines,
        const std::vector<CallSiteInfo>& calls,
        const std::unordered_map<std::string, FunctionInfo>& functions,
        const IndirectCallConfig& config) override {

        IndirectCallResult result;
        result.transformed_code = lines;

        // Filter calls to transform
        std::vector<CallSiteInfo> to_transform;
        for (const auto& call : calls) {
            if (shouldTransformCall(call, config)) {
                if (GlobalRandom::nextDouble() < config.probability) {
                    to_transform.push_back(call);
                }
            }
        }

        if (to_transform.empty()) {
            result.success = true;
            return result;
        }

        // Build function table
        result.table = buildFunctionTable(to_transform, functions, config);
        result.functions_in_table = static_cast<int>(result.table.real_functions);
        result.decoy_entries_added = static_cast<int>(
            result.table.table_size - result.table.real_functions);

        // Create index map: function_name -> table_index
        std::unordered_map<std::string, int> func_to_index;
        for (const auto& entry : result.table.entries) {
            if (!entry.is_decoy) {
                func_to_index[entry.function_name] = entry.index;
            }
        }

        // Track replacements
        std::vector<std::pair<int, std::vector<std::string>>> replacements;

        // Transform each call
        for (const auto& call : to_transform) {
            auto it = func_to_index.find(call.callee_function);
            if (it == func_to_index.end()) continue;

            int table_index = it->second;
            const auto* entry = result.table.getEntryByIndex(table_index);
            if (!entry) continue;

            // Generate replacement code
            std::vector<std::string> replacement = generateIndirectCall(
                call, *entry, result.table, functions, config);

            replacements.push_back({call.line_number, replacement});
            result.calls_transformed++;
        }

        // Apply replacements in reverse order
        std::sort(replacements.begin(), replacements.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        for (const auto& [line_num, replacement] : replacements) {
            if (line_num >= 0 && line_num < static_cast<int>(result.transformed_code.size())) {
                result.transformed_code.erase(
                    result.transformed_code.begin() + line_num);
                result.transformed_code.insert(
                    result.transformed_code.begin() + line_num,
                    replacement.begin(), replacement.end());
            }
        }

        // Insert function table declaration
        insertTableDeclaration(result, functions);

        result.success = true;
        return result;
    }

protected:
    std::vector<std::string> generateAddressDecode(
        const FunctionTableEntry& entry,
        const std::string& encoded_ptr,
        const std::string& decoded_ptr,
        const IndirectCallConfig& config) override {

        std::vector<std::string> code;

        // Convert pointer to integer for manipulation
        std::string ptr_int = "%" + nextTemp();
        code.push_back("  " + ptr_int + " = ptrtoint i8* " + encoded_ptr + " to i64");

        std::string current = ptr_int;

        switch (config.address_strategy) {
            case AddressObfStrategy::None:
                // Just convert back
                code.push_back("  " + decoded_ptr + " = inttoptr i64 " +
                    current + " to i8*");
                return code;

            case AddressObfStrategy::XOR: {
                std::string xored = "%" + nextTemp();
                code.push_back("  " + xored + " = xor i64 " + current +
                    ", " + std::to_string(entry.xor_key));
                current = xored;
                break;
            }

            case AddressObfStrategy::Add: {
                std::string added = "%" + nextTemp();
                code.push_back("  " + added + " = sub i64 " + current +
                    ", " + std::to_string(entry.add_offset));
                current = added;
                break;
            }

            case AddressObfStrategy::XORAdd: {
                std::string xored = "%" + nextTemp();
                std::string added = "%" + nextTemp();
                code.push_back("  " + xored + " = xor i64 " + current +
                    ", " + std::to_string(entry.xor_key));
                code.push_back("  " + added + " = sub i64 " + xored +
                    ", " + std::to_string(entry.add_offset));
                current = added;
                break;
            }

            case AddressObfStrategy::RotateXOR: {
                // Rotate right then XOR
                std::string rotated = "%" + nextTemp();
                std::string xored = "%" + nextTemp();
                std::string shr = "%" + nextTemp();
                std::string shl = "%" + nextTemp();

                int rot = entry.rotate_bits;
                code.push_back("  " + shr + " = lshr i64 " + current +
                    ", " + std::to_string(rot));
                code.push_back("  " + shl + " = shl i64 " + current +
                    ", " + std::to_string(64 - rot));
                code.push_back("  " + rotated + " = or i64 " + shr + ", " + shl);
                code.push_back("  " + xored + " = xor i64 " + rotated +
                    ", " + std::to_string(entry.xor_key));
                current = xored;
                break;
            }
        }

        // Convert back to pointer
        code.push_back("  " + decoded_ptr + " = inttoptr i64 " + current + " to i8*");

        return code;
    }

private:
    /**
     * Generate indirect call code
     */
    std::vector<std::string> generateIndirectCall(
        const CallSiteInfo& call,
        const FunctionTableEntry& entry,
        const FunctionTable& table,
        const std::unordered_map<std::string, FunctionInfo>& functions,
        const IndirectCallConfig& config) {

        std::vector<std::string> code;

        code.push_back("  ; Indirect call to @" + call.callee_function);

        // Load pointer from table
        std::string ptr_var = "%" + nextTemp();
        std::string table_ref = "@" + table.table_name;

        code.push_back("  " + ptr_var + " = load i8*, i8** getelementptr inbounds ([" +
            std::to_string(table.table_size) + " x i8*], [" +
            std::to_string(table.table_size) + " x i8*]* " + table_ref +
            ", i64 0, i64 " + std::to_string(entry.index) + ")");

        // Decode address if obfuscated
        std::string decoded_ptr = ptr_var;
        if (config.address_strategy != AddressObfStrategy::None &&
            config.use_runtime_decode) {
            decoded_ptr = "%" + nextTemp();
            auto decode_code = generateAddressDecode(entry, ptr_var, decoded_ptr, config);
            code.insert(code.end(), decode_code.begin(), decode_code.end());
        }

        // Cast to correct function type
        std::string func_ptr = "%" + nextTemp();
        std::string func_type = getFunctionType(call, functions);

        code.push_back("  " + func_ptr + " = bitcast i8* " + decoded_ptr +
            " to " + func_type + "*");

        // Build the call instruction
        std::string call_instr;
        if (!call.result_var.empty()) {
            call_instr = "  " + call.result_var + " = ";
        } else {
            call_instr = "  ";
        }

        if (call.is_tail_call) {
            call_instr += "tail ";
        }
        if (call.is_must_tail) {
            call_instr += "musttail ";
        }

        call_instr += "call " + call.callee_type + " " + func_ptr + "(";

        // Add arguments
        for (size_t i = 0; i < call.arguments.size(); i++) {
            if (i > 0) call_instr += ", ";
            call_instr += call.arguments[i];
        }
        call_instr += ")";

        // Add attributes if present
        if (!call.call_attributes.empty()) {
            call_instr += " " + trimString(call.call_attributes);
        }

        code.push_back(call_instr);

        return code;
    }

    /**
     * Get function type string for a call
     */
    std::string getFunctionType(
        const CallSiteInfo& call,
        const std::unordered_map<std::string, FunctionInfo>& functions) {

        auto it = functions.find(call.callee_function);
        if (it != functions.end()) {
            return it->second.full_signature;
        }

        // Build from call info
        std::string type = call.callee_type + " (";
        for (size_t i = 0; i < call.arguments.size(); i++) {
            if (i > 0) type += ", ";
            // Extract type from "type value"
            const std::string& arg = call.arguments[i];
            size_t space_pos = arg.find(' ');
            if (space_pos != std::string::npos) {
                type += arg.substr(0, space_pos);
            }
        }
        type += ")";
        return type;
    }

    /**
     * Insert function table declaration at module level
     */
    void insertTableDeclaration(IndirectCallResult& result,
        const std::unordered_map<std::string, FunctionInfo>& functions) {

        std::vector<std::string> declarations;

        const auto& table = result.table;

        // Build table initializer
        declarations.push_back("; Function pointer table");

        std::string entries_str;
        for (size_t i = 0; i < table.entries.size(); i++) {
            const auto& entry = table.entries[i];
            if (i > 0) entries_str += ", ";

            // Get function type for bitcast
            auto it = functions.find(entry.function_name);
            if (it != functions.end()) {
                entries_str += "i8* bitcast (" + it->second.full_signature +
                    "* @" + entry.function_name + " to i8*)";
            } else {
                entries_str += "i8* bitcast (i8* @" + entry.function_name + " to i8*)";
            }
        }

        declarations.push_back("@" + table.table_name +
            " = private unnamed_addr global [" +
            std::to_string(table.table_size) + " x i8*] [" + entries_str + "]");

        // Find insertion point (before first function definition)
        size_t insert_pos = 0;
        for (size_t i = 0; i < result.transformed_code.size(); i++) {
            if (result.transformed_code[i].find("define ") != std::string::npos) {
                insert_pos = i;
                break;
            }
        }

        result.transformed_code.insert(
            result.transformed_code.begin() + insert_pos,
            declarations.begin(), declarations.end());
    }

    static std::string trimString(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
};

/**
 * LLVM IR Indirect Call Pass
 */
class LLVMIndirectCallPass : public LLVMTransformationPass {
public:
    LLVMIndirectCallPass() : analyzer_(), transformer_() {}

    std::string getName() const override { return "IndirectCall"; }
    std::string getDescription() const override {
        return "Converts direct function calls to indirect calls via pointer tables";
    }

    PassPriority getPriority() const override { return PassPriority::ControlFlow; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        ic_config_.enabled = config.enabled;
        ic_config_.probability = config.probability;
        return true;
    }

    void setIndirectCallConfig(const IndirectCallConfig& config) {
        ic_config_ = config;
    }

    const IndirectCallConfig& getConfig() const {
        return ic_config_;
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!ic_config_.enabled) {
            return TransformResult::Skipped;
        }

        // Find all calls
        auto calls = analyzer_.findCalls(lines);
        statistics_["calls_found"] = static_cast<int>(calls.size());

        // Extract function information
        auto functions = analyzer_.extractFunctions(lines);
        statistics_["functions_found"] = static_cast<int>(functions.size());

        // Transform calls
        auto ic_result = transformer_.transform(lines, calls, functions, ic_config_);

        if (ic_result.success) {
            lines = std::move(ic_result.transformed_code);
            statistics_["calls_transformed"] = ic_result.calls_transformed;
            statistics_["functions_in_table"] = ic_result.functions_in_table;
            statistics_["decoy_entries"] = ic_result.decoy_entries_added;
            return TransformResult::Success;
        } else {
            return TransformResult::Error;
        }
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    LLVMCallSiteAnalyzer analyzer_;
    LLVMIndirectCallTransformation transformer_;
    IndirectCallConfig ic_config_;
};

} // namespace control_flow
} // namespace morphect

#endif // MORPHECT_INDIRECT_CALL_HPP
