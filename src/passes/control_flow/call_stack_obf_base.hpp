/**
 * Morphect - Call Stack Obfuscation Base
 *
 * Base definitions for call stack obfuscation transformations.
 * Includes call proxying and fake call insertion.
 *
 * Call Proxying:
 *   Original: target(args);
 *   Proxied:  _proxy_N((void*)target, args);
 *
 * Fake Calls:
 *   Insert dead calls guarded by opaque predicates that never execute.
 */

#ifndef MORPHECT_CALL_STACK_OBF_BASE_HPP
#define MORPHECT_CALL_STACK_OBF_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace morphect {
namespace control_flow {

/**
 * Type of proxy function
 */
enum class ProxyType {
    Simple,         // Direct cast and call
    Trampoline,     // Jump through trampoline
    Dispatcher      // Central dispatcher with switch
};

/**
 * Type of fake call
 */
enum class FakeCallType {
    NeverReached,   // Guarded by always-false condition
    DeadBranch,     // In unreachable code path
    Decoy           // Looks real but result unused
};

/**
 * Information about a proxy function
 */
struct ProxyFunctionInfo {
    std::string name;                // Proxy function name
    std::string target_type;         // Function pointer type
    std::string return_type;         // Return type
    std::vector<std::string> param_types;  // Parameter types (excluding func ptr)
    ProxyType type = ProxyType::Simple;

    // For dispatcher type
    int dispatch_id = -1;            // ID in dispatch table
};

/**
 * Information about a fake call to insert
 */
struct FakeCallInfo {
    std::string target_function;     // Function to "call"
    std::vector<std::string> arguments;  // Fake arguments
    FakeCallType type = FakeCallType::NeverReached;

    // Guard information
    std::string guard_condition;     // Opaque predicate (always false)
    std::string guard_var;           // Variable for condition

    // Location
    int insert_line = -1;            // Where to insert
    std::string insert_block;        // Block to insert in
};

/**
 * Configuration for call stack obfuscation
 */
struct CallStackObfConfig {
    bool enabled = true;
    double proxy_probability = 0.7;  // Probability of proxying a call
    double fake_call_probability = 0.3;  // Probability of adding fake call

    // Proxy options
    ProxyType proxy_type = ProxyType::Simple;
    bool use_single_dispatcher = false;  // All calls through one dispatcher
    std::string proxy_prefix = "_proxy_";

    // Fake call options
    int min_fake_calls = 1;
    int max_fake_calls = 3;
    bool use_real_functions = true;  // Fake calls target real functions

    // Function filtering
    std::vector<std::string> exclude_functions;
    bool skip_intrinsics = true;
    bool skip_external = false;      // Skip external function calls

    // Opaque predicate options for fake calls
    bool use_arithmetic_predicates = true;  // x*x >= 0 style
    bool use_pointer_predicates = false;    // &x != NULL style
};

/**
 * Result of call stack obfuscation
 */
struct CallStackObfResult {
    bool success = false;
    std::string error;

    int calls_proxied = 0;
    int fake_calls_added = 0;
    int proxy_functions_created = 0;

    std::vector<ProxyFunctionInfo> proxies;
    std::vector<std::string> transformed_code;
};

/**
 * Opaque predicate generator for fake calls
 */
class OpaquePredicateGenerator {
public:
    /**
     * Generate an always-false predicate
     */
    static std::pair<std::string, std::vector<std::string>> generateAlwaysFalse(
        const std::string& var_prefix) {

        int choice = GlobalRandom::nextInt(0, 4);
        std::string var = "%" + var_prefix + "_op" + std::to_string(GlobalRandom::nextInt(0, 1000));
        std::vector<std::string> setup_code;
        std::string condition;

        switch (choice) {
            case 0: {
                // x * x < 0 is always false for integers
                std::string sq_var = var + "_sq";
                setup_code.push_back("  " + var + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(1, 100)));
                setup_code.push_back("  " + sq_var + " = mul i32 " + var + ", " + var);
                condition = "icmp slt i32 " + sq_var + ", 0";
                break;
            }
            case 1: {
                // (x & 1) == 2 is always false
                std::string and_var = var + "_and";
                setup_code.push_back("  " + var + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup_code.push_back("  " + and_var + " = and i32 " + var + ", 1");
                condition = "icmp eq i32 " + and_var + ", 2";
                break;
            }
            case 2: {
                // x == x + 1 is always false
                std::string plus_var = var + "_plus";
                setup_code.push_back("  " + var + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup_code.push_back("  " + plus_var + " = add i32 " + var + ", 1");
                condition = "icmp eq i32 " + var + ", " + plus_var;
                break;
            }
            case 3: {
                // (x | x) != x is always false
                std::string or_var = var + "_or";
                setup_code.push_back("  " + var + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup_code.push_back("  " + or_var + " = or i32 " + var + ", " + var);
                condition = "icmp ne i32 " + or_var + ", " + var;
                break;
            }
            default: {
                // Fallback: 0 == 1
                condition = "icmp eq i32 0, 1";
                break;
            }
        }

        return {condition, setup_code};
    }

    /**
     * Generate an always-true predicate (for real code paths)
     */
    static std::pair<std::string, std::vector<std::string>> generateAlwaysTrue(
        const std::string& var_prefix) {

        int choice = GlobalRandom::nextInt(0, 3);
        std::string var = "%" + var_prefix + "_op" + std::to_string(GlobalRandom::nextInt(0, 1000));
        std::vector<std::string> setup_code;
        std::string condition;

        switch (choice) {
            case 0: {
                // x * x >= 0 is always true
                std::string sq_var = var + "_sq";
                setup_code.push_back("  " + var + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(1, 100)));
                setup_code.push_back("  " + sq_var + " = mul i32 " + var + ", " + var);
                condition = "icmp sge i32 " + sq_var + ", 0";
                break;
            }
            case 1: {
                // x == x is always true
                setup_code.push_back("  " + var + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                condition = "icmp eq i32 " + var + ", " + var;
                break;
            }
            case 2: {
                // (x | 0) == x is always true
                std::string or_var = var + "_or";
                setup_code.push_back("  " + var + " = add i32 0, " +
                    std::to_string(GlobalRandom::nextInt(0, 1000)));
                setup_code.push_back("  " + or_var + " = or i32 " + var + ", 0");
                condition = "icmp eq i32 " + or_var + ", " + var;
                break;
            }
            default: {
                // Fallback: 1 == 1
                condition = "icmp eq i32 1, 1";
                break;
            }
        }

        return {condition, setup_code};
    }
};

/**
 * Base class for call stack obfuscation
 */
class CallStackObfTransformation {
public:
    virtual ~CallStackObfTransformation() = default;

    /**
     * Get transformation name
     */
    virtual std::string getName() const = 0;

    /**
     * Transform calls with proxying and fake calls
     */
    virtual CallStackObfResult transform(
        const std::vector<std::string>& lines,
        const CallStackObfConfig& config) = 0;

protected:
    Logger logger_{"CallStackObf"};
    int proxy_counter_ = 0;
    int temp_counter_ = 0;
    int label_counter_ = 0;

    std::string nextProxyName(const std::string& prefix) {
        return prefix + std::to_string(proxy_counter_++);
    }

    std::string nextTemp() {
        return "_cso_tmp" + std::to_string(temp_counter_++);
    }

    std::string nextLabel() {
        return "_cso_bb" + std::to_string(label_counter_++);
    }

    /**
     * Check if a function should be excluded
     */
    bool shouldExclude(const std::string& func_name,
                      const CallStackObfConfig& config) {
        if (config.skip_intrinsics && func_name.find("llvm.") == 0) {
            return true;
        }

        for (const auto& excl : config.exclude_functions) {
            if (func_name == excl) {
                return true;
            }
        }

        return false;
    }
};

} // namespace control_flow
} // namespace morphect

#endif // MORPHECT_CALL_STACK_OBF_BASE_HPP
