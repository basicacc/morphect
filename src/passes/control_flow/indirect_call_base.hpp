/**
 * Morphect - Indirect Call Obfuscation Base
 *
 * Base definitions for indirect call transformations.
 * Converts direct function calls to indirect calls via function pointer tables.
 *
 * Original:
 *   call @function(args)
 *
 * Transformed:
 *   %ptr = load i8*, i8** getelementptr @func_table, idx
 *   %decoded = xor %ptr, key  ; optional address obfuscation
 *   %func = bitcast %decoded to func_type*
 *   call %func(args)
 */

#ifndef MORPHECT_INDIRECT_CALL_BASE_HPP
#define MORPHECT_INDIRECT_CALL_BASE_HPP

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
 * Strategy for obfuscating function addresses
 */
enum class AddressObfStrategy {
    None,           // No obfuscation (just indirection)
    XOR,            // XOR with key
    Add,            // Add constant offset
    XORAdd,         // XOR then add
    RotateXOR       // Rotate bits then XOR
};

/**
 * Information about a function to be indirected
 */
struct FunctionInfo {
    std::string name;                // Function name
    std::string return_type;         // Return type (void, i32, etc.)
    std::vector<std::string> param_types;  // Parameter types
    std::string full_signature;      // Full function type string

    // For varargs functions
    bool is_vararg = false;

    // Linkage information
    bool is_external = false;        // Defined externally
    bool is_declaration = false;     // Just a declaration

    // Table assignment
    int table_index = -1;            // Index in function pointer table
    int obfuscated_index = -1;       // Obfuscated index
};

/**
 * Entry in the function pointer table
 */
struct FunctionTableEntry {
    int index = 0;                   // Position in table
    std::string function_name;       // Target function
    std::string function_type;       // Function signature type

    // Address obfuscation
    uint64_t xor_key = 0;
    int64_t add_offset = 0;
    int rotate_bits = 0;

    // Decoy information
    bool is_decoy = false;
    std::string decoy_target;        // Where decoy points (valid function for safety)
};

/**
 * Function pointer table
 */
struct FunctionTable {
    std::string table_name;
    std::vector<FunctionTableEntry> entries;

    // Obfuscation settings
    AddressObfStrategy address_strategy = AddressObfStrategy::XOR;
    uint64_t global_xor_key = 0;     // Global key for all entries

    // Table properties
    size_t table_size = 0;
    size_t real_functions = 0;
    bool has_decoys = false;

    /**
     * Get entry by function name
     */
    const FunctionTableEntry* getEntry(const std::string& name) const {
        for (const auto& entry : entries) {
            if (entry.function_name == name) {
                return &entry;
            }
        }
        return nullptr;
    }

    /**
     * Get entry by index
     */
    const FunctionTableEntry* getEntryByIndex(int idx) const {
        for (const auto& entry : entries) {
            if (entry.index == idx) {
                return &entry;
            }
        }
        return nullptr;
    }
};

/**
 * Information about a call site to transform
 */
struct CallSiteInfo {
    int id;                          // Unique ID
    std::string caller_function;     // Function containing the call
    std::string callee_function;     // Function being called
    std::string callee_type;         // Function type signature

    // Call instruction details
    std::string result_var;          // Variable receiving return value (if any)
    std::vector<std::string> arguments;  // Call arguments
    std::string original_instruction;
    int line_number = -1;

    // Call attributes
    std::string call_attributes;     // nounwind, readonly, etc.
    bool is_tail_call = false;
    bool is_must_tail = false;
};

/**
 * Configuration for indirect call pass
 */
struct IndirectCallConfig {
    bool enabled = true;
    double probability = 0.75;       // Probability of transforming a call

    // Address obfuscation
    AddressObfStrategy address_strategy = AddressObfStrategy::XOR;
    bool use_runtime_decode = true;  // Decode at each call site

    // Table options
    bool add_decoy_entries = true;
    int min_decoy_count = 1;
    int max_decoy_count = 3;
    bool shuffle_entries = true;

    // Function filtering
    std::vector<std::string> exclude_functions;  // Don't redirect these
    std::vector<std::string> include_only;       // Only redirect these (if non-empty)
    bool skip_external = false;      // Skip calls to external functions
    bool skip_intrinsics = true;     // Skip LLVM intrinsics

    // Naming
    std::string table_name = "_func_table";
    std::string decoder_prefix = "_decode_addr_";
};

/**
 * Result of indirect call transformation
 */
struct IndirectCallResult {
    bool success = false;
    std::string error;

    int calls_transformed = 0;
    int functions_in_table = 0;
    int decoy_entries_added = 0;

    FunctionTable table;
    std::vector<std::string> transformed_code;
};

/**
 * Base class for call site analysis
 */
class CallSiteAnalyzer {
public:
    virtual ~CallSiteAnalyzer() = default;

    /**
     * Find all direct calls in the code
     */
    virtual std::vector<CallSiteInfo> findCalls(
        const std::vector<std::string>& lines) = 0;

    /**
     * Extract function declarations and definitions
     */
    virtual std::unordered_map<std::string, FunctionInfo> extractFunctions(
        const std::vector<std::string>& lines) = 0;

    /**
     * Check if a call should be transformed
     */
    virtual bool shouldTransform(const CallSiteInfo& call,
                                const IndirectCallConfig& config) {
        // Skip intrinsics
        if (config.skip_intrinsics &&
            call.callee_function.find("llvm.") == 0) {
            return false;
        }

        // Check exclusion list
        for (const auto& excl : config.exclude_functions) {
            if (call.callee_function == excl) {
                return false;
            }
        }

        // Check inclusion list
        if (!config.include_only.empty()) {
            bool found = false;
            for (const auto& incl : config.include_only) {
                if (call.callee_function == incl) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }

        return true;
    }
};

/**
 * Base class for indirect call transformation
 */
class IndirectCallTransformation {
public:
    virtual ~IndirectCallTransformation() = default;

    /**
     * Get transformation name
     */
    virtual std::string getName() const = 0;

    /**
     * Transform direct calls to indirect calls
     */
    virtual IndirectCallResult transform(
        const std::vector<std::string>& lines,
        const std::vector<CallSiteInfo>& calls,
        const std::unordered_map<std::string, FunctionInfo>& functions,
        const IndirectCallConfig& config) = 0;

protected:
    Logger logger_{"IndirectCall"};
    int temp_counter_ = 0;

    std::string nextTemp() {
        return "_ic_tmp" + std::to_string(temp_counter_++);
    }

    /**
     * Build function pointer table
     */
    virtual FunctionTable buildFunctionTable(
        const std::vector<CallSiteInfo>& calls,
        const std::unordered_map<std::string, FunctionInfo>& functions,
        const IndirectCallConfig& config) {

        FunctionTable table;
        table.table_name = config.table_name;
        table.address_strategy = config.address_strategy;

        // Collect unique called functions
        std::unordered_set<std::string> called_functions;
        for (const auto& call : calls) {
            if (shouldTransformCall(call, config)) {
                called_functions.insert(call.callee_function);
            }
        }

        // Create entries
        int index = 0;
        for (const auto& func_name : called_functions) {
            FunctionTableEntry entry;
            entry.index = index++;
            entry.function_name = func_name;

            // Get function type from map
            auto it = functions.find(func_name);
            if (it != functions.end()) {
                entry.function_type = it->second.full_signature;
            }

            entry.is_decoy = false;
            table.entries.push_back(entry);
        }

        table.real_functions = table.entries.size();

        // Add decoy entries
        if (config.add_decoy_entries && !table.entries.empty()) {
            int decoy_count = GlobalRandom::nextInt(
                config.min_decoy_count, config.max_decoy_count + 1);

            for (int i = 0; i < decoy_count; i++) {
                FunctionTableEntry decoy;
                decoy.index = index++;
                decoy.is_decoy = true;
                // Point to a random real function for safety
                int target_idx = GlobalRandom::nextInt(0,
                    static_cast<int>(table.real_functions));
                decoy.function_name = table.entries[target_idx].function_name;
                decoy.function_type = table.entries[target_idx].function_type;
                decoy.decoy_target = decoy.function_name;
                table.entries.push_back(decoy);
            }
            table.has_decoys = true;
        }

        table.table_size = table.entries.size();

        // Apply address obfuscation
        applyAddressObfuscation(table, config);

        // Shuffle entries if configured
        if (config.shuffle_entries) {
            shuffleEntries(table);
        }

        return table;
    }

    /**
     * Check if a specific call should be transformed
     */
    bool shouldTransformCall(const CallSiteInfo& call,
                            const IndirectCallConfig& config) {
        // Skip intrinsics
        if (config.skip_intrinsics &&
            call.callee_function.find("llvm.") == 0) {
            return false;
        }

        // Check exclusion list
        for (const auto& excl : config.exclude_functions) {
            if (call.callee_function == excl) {
                return false;
            }
        }

        return true;
    }

    /**
     * Apply address obfuscation to table entries
     */
    virtual void applyAddressObfuscation(FunctionTable& table,
                                         const IndirectCallConfig& config) {
        // Generate global key
        table.global_xor_key = static_cast<uint64_t>(
            GlobalRandom::nextInt(0x1000, 0x7FFFFFFF));

        for (auto& entry : table.entries) {
            switch (config.address_strategy) {
                case AddressObfStrategy::None:
                    break;

                case AddressObfStrategy::XOR:
                    entry.xor_key = table.global_xor_key;
                    break;

                case AddressObfStrategy::Add:
                    entry.add_offset = GlobalRandom::nextInt(-1000, 1000);
                    break;

                case AddressObfStrategy::XORAdd:
                    entry.xor_key = table.global_xor_key;
                    entry.add_offset = GlobalRandom::nextInt(-100, 100);
                    break;

                case AddressObfStrategy::RotateXOR:
                    entry.xor_key = table.global_xor_key;
                    entry.rotate_bits = GlobalRandom::nextInt(1, 15);
                    break;
            }
        }
    }

    /**
     * Shuffle table entries while maintaining index mapping
     */
    void shuffleEntries(FunctionTable& table) {
        // Fisher-Yates shuffle
        for (size_t i = table.entries.size() - 1; i > 0; i--) {
            size_t j = GlobalRandom::nextInt(0, static_cast<int>(i));
            std::swap(table.entries[i], table.entries[j]);
        }

        // Reassign indices after shuffle
        for (size_t i = 0; i < table.entries.size(); i++) {
            table.entries[i].index = static_cast<int>(i);
        }
    }

    /**
     * Generate code to decode function address
     */
    virtual std::vector<std::string> generateAddressDecode(
        const FunctionTableEntry& entry,
        const std::string& encoded_ptr,
        const std::string& decoded_ptr,
        const IndirectCallConfig& config) = 0;
};

} // namespace control_flow
} // namespace morphect

#endif // MORPHECT_INDIRECT_CALL_BASE_HPP
