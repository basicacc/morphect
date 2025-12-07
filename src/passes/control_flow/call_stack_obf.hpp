/**
 * Morphect - Call Stack Obfuscation for LLVM IR
 *
 * Implements call proxying and fake call insertion.
 *
 * Call Proxying:
 *   Original: %r = call i32 @func(i32 %x)
 *   Proxied:  %r = call i32 @_proxy_0(i8* bitcast (i32 (i32)* @func to i8*), i32 %x)
 *
 * Fake Calls:
 *   ; Opaque predicate (always false)
 *   %op = mul i32 %v, %v
 *   %cond = icmp slt i32 %op, 0
 *   br i1 %cond, label %fake, label %real
 *   fake:
 *     call void @decoy_func()  ; Never executed
 *     br label %real
 *   real:
 *     ; Continue normal execution
 */

#ifndef MORPHECT_CALL_STACK_OBF_HPP
#define MORPHECT_CALL_STACK_OBF_HPP

#include "call_stack_obf_base.hpp"
#include "indirect_call.hpp"

#include <regex>
#include <sstream>
#include <algorithm>

namespace morphect {
namespace control_flow {

/**
 * LLVM IR Call Stack Obfuscation Transformation
 */
class LLVMCallStackObfTransformation : public CallStackObfTransformation {
public:
    std::string getName() const override { return "LLVM_CallStackObf"; }

    CallStackObfResult transform(
        const std::vector<std::string>& lines,
        const CallStackObfConfig& config) override {

        CallStackObfResult result;
        result.transformed_code = lines;

        // Analyze the code to find calls and functions
        LLVMCallSiteAnalyzer analyzer;
        auto calls = analyzer.findCalls(lines);
        auto functions = analyzer.extractFunctions(lines);

        // Collect function names for fake calls
        std::vector<std::string> available_functions;
        for (const auto& [name, info] : functions) {
            if (!shouldExclude(name, config)) {
                available_functions.push_back(name);
            }
        }

        // Track proxies to create
        std::unordered_map<std::string, ProxyFunctionInfo> proxies_needed;

        // Track replacements
        std::vector<std::pair<int, std::vector<std::string>>> replacements;

        // Process calls for proxying
        for (const auto& call : calls) {
            if (shouldExclude(call.callee_function, config)) {
                continue;
            }

            if (GlobalRandom::nextDouble() > config.proxy_probability) {
                continue;
            }

            // Create proxy for this call signature
            std::string proxy_key = getProxyKey(call, functions);
            ProxyFunctionInfo proxy;

            if (proxies_needed.find(proxy_key) == proxies_needed.end()) {
                proxy = createProxyInfo(call, functions, config);
                proxies_needed[proxy_key] = proxy;
            } else {
                proxy = proxies_needed[proxy_key];
            }

            // Generate proxied call
            auto proxied = generateProxiedCall(call, proxy, functions, config);
            replacements.push_back({call.line_number, proxied});
            result.calls_proxied++;
        }

        // Apply call replacements in reverse order
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

        // Insert fake calls
        if (!available_functions.empty()) {
            insertFakeCalls(result.transformed_code, available_functions,
                           functions, config, result.fake_calls_added);
        }

        // Insert proxy function definitions
        for (const auto& [key, proxy] : proxies_needed) {
            auto proxy_def = generateProxyDefinition(proxy, config);
            insertProxyDefinition(result.transformed_code, proxy_def);
            result.proxies.push_back(proxy);
            result.proxy_functions_created++;
        }

        result.success = true;
        return result;
    }

private:
    /**
     * Get a unique key for proxy function based on signature
     */
    std::string getProxyKey(const CallSiteInfo& call,
                           const std::unordered_map<std::string, FunctionInfo>& functions) {
        std::string key = call.callee_type + "(";
        for (size_t i = 0; i < call.arguments.size(); i++) {
            if (i > 0) key += ",";
            // Extract type from argument
            const auto& arg = call.arguments[i];
            size_t space = arg.find(' ');
            if (space != std::string::npos) {
                key += arg.substr(0, space);
            }
        }
        key += ")";
        return key;
    }

    /**
     * Create proxy function info
     */
    ProxyFunctionInfo createProxyInfo(
        const CallSiteInfo& call,
        const std::unordered_map<std::string, FunctionInfo>& functions,
        const CallStackObfConfig& config) {

        ProxyFunctionInfo proxy;
        proxy.name = nextProxyName(config.proxy_prefix);
        proxy.return_type = call.callee_type;
        proxy.type = config.proxy_type;

        // Build function pointer type
        proxy.target_type = call.callee_type + " (";
        for (size_t i = 0; i < call.arguments.size(); i++) {
            if (i > 0) proxy.target_type += ", ";
            const auto& arg = call.arguments[i];
            size_t space = arg.find(' ');
            if (space != std::string::npos) {
                std::string type = arg.substr(0, space);
                proxy.target_type += type;
                proxy.param_types.push_back(type);
            }
        }
        proxy.target_type += ")*";

        return proxy;
    }

    /**
     * Generate proxied call code
     */
    std::vector<std::string> generateProxiedCall(
        const CallSiteInfo& call,
        const ProxyFunctionInfo& proxy,
        const std::unordered_map<std::string, FunctionInfo>& functions,
        const CallStackObfConfig& config) {

        std::vector<std::string> code;
        code.push_back("  ; Proxied call to @" + call.callee_function);

        // Cast function to i8*
        std::string cast_var = "%" + nextTemp();
        code.push_back("  " + cast_var + " = bitcast " + proxy.target_type +
            " @" + call.callee_function + " to i8*");

        // Build call to proxy
        std::string call_str;
        if (!call.result_var.empty()) {
            call_str = "  " + call.result_var + " = ";
        } else {
            call_str = "  ";
        }

        if (call.is_tail_call) {
            call_str += "tail ";
        }

        call_str += "call " + proxy.return_type + " @" + proxy.name + "(i8* " + cast_var;

        // Add original arguments
        for (const auto& arg : call.arguments) {
            call_str += ", " + arg;
        }
        call_str += ")";

        code.push_back(call_str);

        return code;
    }

    /**
     * Generate proxy function definition
     */
    std::vector<std::string> generateProxyDefinition(
        const ProxyFunctionInfo& proxy,
        const CallStackObfConfig& config) {

        std::vector<std::string> code;

        // Build parameter list
        std::string params = "i8* %_func_ptr";
        for (size_t i = 0; i < proxy.param_types.size(); i++) {
            params += ", " + proxy.param_types[i] + " %_arg" + std::to_string(i);
        }

        code.push_back("");
        code.push_back("; Proxy function for " + proxy.return_type + " calls");
        code.push_back("define internal " + proxy.return_type + " @" +
            proxy.name + "(" + params + ") {");
        code.push_back("entry:");

        // Cast function pointer
        std::string func_type = proxy.return_type + " (";
        for (size_t i = 0; i < proxy.param_types.size(); i++) {
            if (i > 0) func_type += ", ";
            func_type += proxy.param_types[i];
        }
        func_type += ")*";

        code.push_back("  %_typed_func = bitcast i8* %_func_ptr to " + func_type);

        // Build call
        std::string call_str;
        if (proxy.return_type != "void") {
            call_str = "  %_result = call " + proxy.return_type +
                " %_typed_func(";
        } else {
            call_str = "  call void %_typed_func(";
        }

        for (size_t i = 0; i < proxy.param_types.size(); i++) {
            if (i > 0) call_str += ", ";
            call_str += proxy.param_types[i] + " %_arg" + std::to_string(i);
        }
        call_str += ")";
        code.push_back(call_str);

        // Return
        if (proxy.return_type != "void") {
            code.push_back("  ret " + proxy.return_type + " %_result");
        } else {
            code.push_back("  ret void");
        }

        code.push_back("}");

        return code;
    }

    /**
     * Insert fake calls into the code
     */
    void insertFakeCalls(
        std::vector<std::string>& code,
        const std::vector<std::string>& available_functions,
        const std::unordered_map<std::string, FunctionInfo>& functions,
        const CallStackObfConfig& config,
        int& fake_calls_added) {

        // Find suitable insertion points (inside function bodies, before terminators)
        std::vector<int> insertion_points;
        bool in_function = false;

        for (size_t i = 0; i < code.size(); i++) {
            const std::string& line = code[i];

            if (line.find("define ") != std::string::npos) {
                in_function = true;
                continue;
            }
            if (line == "}") {
                in_function = false;
                continue;
            }

            // Insert before branch/ret instructions
            if (in_function &&
                (line.find("  br ") != std::string::npos ||
                 line.find("  ret ") != std::string::npos) &&
                line.find("  br i1") == std::string::npos) {  // Not conditional branch
                insertion_points.push_back(static_cast<int>(i));
            }
        }

        if (insertion_points.empty()) return;

        // Determine how many fake calls to add
        int num_fake = GlobalRandom::nextInt(config.min_fake_calls,
            std::min(config.max_fake_calls + 1,
                    static_cast<int>(insertion_points.size()) + 1));

        // Shuffle and select insertion points
        std::vector<int> selected;
        for (int i = 0; i < static_cast<int>(insertion_points.size()); i++) {
            selected.push_back(i);
        }
        for (size_t i = selected.size() - 1; i > 0; i--) {
            size_t j = GlobalRandom::nextInt(0, static_cast<int>(i));
            std::swap(selected[i], selected[j]);
        }

        // Insert fake calls (in reverse order to preserve line numbers)
        std::vector<int> points_to_use;
        for (int i = 0; i < num_fake && i < static_cast<int>(selected.size()); i++) {
            points_to_use.push_back(insertion_points[selected[i]]);
        }
        std::sort(points_to_use.rbegin(), points_to_use.rend());

        for (int insert_line : points_to_use) {
            if (GlobalRandom::nextDouble() > config.fake_call_probability) {
                continue;
            }

            // Pick a random function to fake-call
            int func_idx = GlobalRandom::nextInt(0, static_cast<int>(available_functions.size()) - 1);
            const std::string& target_func = available_functions[func_idx];

            auto it = functions.find(target_func);
            if (it == functions.end()) continue;

            // Generate fake call with opaque predicate
            auto fake_code = generateFakeCall(target_func, it->second, config);

            code.insert(code.begin() + insert_line, fake_code.begin(), fake_code.end());
            fake_calls_added++;
        }
    }

    /**
     * Generate fake call guarded by opaque predicate
     */
    std::vector<std::string> generateFakeCall(
        const std::string& target_func,
        const FunctionInfo& func_info,
        const CallStackObfConfig& config) {

        std::vector<std::string> code;

        std::string prefix = "_fake" + std::to_string(GlobalRandom::nextInt(0, 10000));

        // Generate opaque predicate (always false)
        auto [condition, setup_code] = OpaquePredicateGenerator::generateAlwaysFalse(prefix);

        // Labels for branching
        std::string fake_label = nextLabel() + "_fake";
        std::string cont_label = nextLabel() + "_cont";

        code.push_back("  ; Fake call (never executed)");

        // Setup code for opaque predicate
        for (const auto& line : setup_code) {
            code.push_back(line);
        }

        // Condition
        std::string cond_var = "%" + prefix + "_cond";
        code.push_back("  " + cond_var + " = " + condition);

        // Branch
        code.push_back("  br i1 " + cond_var + ", label %" + fake_label +
            ", label %" + cont_label);

        // Fake block (never reached)
        code.push_back(fake_label + ":");

        // Generate fake call
        std::string fake_call = "  call " + func_info.return_type + " @" + target_func + "(";
        for (size_t i = 0; i < func_info.param_types.size(); i++) {
            if (i > 0) fake_call += ", ";
            fake_call += func_info.param_types[i] + " ";
            // Generate dummy argument
            if (func_info.param_types[i].find("i32") != std::string::npos) {
                fake_call += std::to_string(GlobalRandom::nextInt(0, 100));
            } else if (func_info.param_types[i].find("i64") != std::string::npos) {
                fake_call += std::to_string(GlobalRandom::nextInt(0, 100));
            } else if (func_info.param_types[i].find("*") != std::string::npos) {
                fake_call += "null";
            } else {
                fake_call += "zeroinitializer";
            }
        }
        fake_call += ")";
        code.push_back(fake_call);
        code.push_back("  br label %" + cont_label);

        // Continue block
        code.push_back(cont_label + ":");

        return code;
    }

    /**
     * Insert proxy definition before first function
     */
    void insertProxyDefinition(std::vector<std::string>& code,
                               const std::vector<std::string>& proxy_def) {
        // Find first function definition
        size_t insert_pos = 0;
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].find("define ") != std::string::npos &&
                code[i].find("define internal") == std::string::npos) {
                insert_pos = i;
                break;
            }
        }

        code.insert(code.begin() + insert_pos, proxy_def.begin(), proxy_def.end());
    }
};

/**
 * LLVM IR Call Stack Obfuscation Pass
 */
class LLVMCallStackObfPass : public LLVMTransformationPass {
public:
    LLVMCallStackObfPass() : transformer_() {}

    std::string getName() const override { return "CallStackObf"; }
    std::string getDescription() const override {
        return "Obfuscates call stack with proxying and fake calls";
    }

    PassPriority getPriority() const override { return PassPriority::ControlFlow; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        cso_config_.enabled = config.enabled;
        cso_config_.proxy_probability = config.probability;
        return true;
    }

    void setCallStackObfConfig(const CallStackObfConfig& config) {
        cso_config_ = config;
    }

    const CallStackObfConfig& getConfig() const {
        return cso_config_;
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!cso_config_.enabled) {
            return TransformResult::Skipped;
        }

        auto cso_result = transformer_.transform(lines, cso_config_);

        if (cso_result.success) {
            lines = std::move(cso_result.transformed_code);
            statistics_["calls_proxied"] = cso_result.calls_proxied;
            statistics_["fake_calls_added"] = cso_result.fake_calls_added;
            statistics_["proxy_functions_created"] = cso_result.proxy_functions_created;
            return TransformResult::Success;
        } else {
            return TransformResult::Error;
        }
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    LLVMCallStackObfTransformation transformer_;
    CallStackObfConfig cso_config_;
};

} // namespace control_flow
} // namespace morphect

#endif // MORPHECT_CALL_STACK_OBF_HPP
