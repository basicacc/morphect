/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * transformation_base.hpp - Base class for all transformation passes
 *
 * All obfuscation passes (MBA, CFF, Bogus CF, etc.) inherit from this
 * base class to ensure consistent interface and behavior.
 */

#ifndef MORPHECT_TRANSFORMATION_BASE_HPP
#define MORPHECT_TRANSFORMATION_BASE_HPP

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace morphect {

// Forward declarations
class Statistics;
class Config;

/**
 * Result of a transformation attempt
 */
enum class TransformResult {
    Success,        // Transformation applied successfully
    Skipped,        // Transformation skipped (probability, config, etc.)
    NotApplicable,  // Pattern didn't match
    Error           // Transformation failed
};

/**
 * Pass priority levels for ordering
 */
enum class PassPriority {
    Early    = 100,   // Run first (e.g., analysis passes)
    Normal   = 500,   // Default priority
    Late     = 900,   // Run last (e.g., cleanup passes)

    // Specific pass priorities
    ControlFlow = 200,  // CFF, Bogus CF run early
    MBA         = 400,  // MBA in middle
    Data        = 600,  // String/constant obfuscation
    Cleanup     = 800   // Dead code, finalization
};

/**
 * Pass type for static dispatch (no RTTI in GCC plugins)
 */
enum class PassType {
    Generic,
    Gimple,
    LLVM,
    Assembly
};

/**
 * Base configuration for all passes
 */
struct PassConfig {
    bool enabled = true;
    double probability = 0.85;  // Global transformation probability
    int verbosity = 1;          // 0=silent, 1=normal, 2=verbose, 3=debug

    // Optional per-function control
    std::vector<std::string> include_functions;  // Only these functions
    std::vector<std::string> exclude_functions;  // Skip these functions
};

/**
 * Abstract base class for all transformation passes
 *
 * Lifecycle:
 *   1. Constructor - basic initialization
 *   2. initialize(config) - load configuration
 *   3. transform(func) - called for each function (multiple times)
 *   4. finalize() - cleanup and final statistics
 */
class TransformationPass {
public:
    virtual ~TransformationPass() = default;

    /**
     * Get the unique name of this pass
     * Used for logging, statistics, and configuration
     */
    virtual std::string getName() const = 0;

    /**
     * Get a description of what this pass does
     */
    virtual std::string getDescription() const = 0;

    /**
     * Get the priority of this pass for ordering
     */
    virtual PassPriority getPriority() const { return PassPriority::Normal; }

    /**
     * Get the type of this pass (for static dispatch without RTTI)
     */
    virtual PassType getPassType() const { return PassType::Generic; }

    /**
     * Get dependencies - passes that must run before this one
     */
    virtual std::vector<std::string> getDependencies() const { return {}; }

    /**
     * Initialize the pass with configuration
     * Called once before any transformations
     *
     * @param config Pass-specific configuration
     * @return true if initialization successful
     */
    virtual bool initialize(const PassConfig& config) {
        config_ = config;
        return true;
    }

    /**
     * Check if this pass is enabled
     */
    bool isEnabled() const { return config_.enabled; }

    /**
     * Set enabled state
     */
    void setEnabled(bool enabled) { config_.enabled = enabled; }

    /**
     * Get current configuration
     */
    const PassConfig& getConfig() const { return config_; }

    /**
     * Finalize the pass after all transformations
     * Called once at the end
     */
    virtual void finalize() {}

    /**
     * Get statistics from this pass
     */
    virtual std::unordered_map<std::string, int> getStatistics() const {
        return statistics_;
    }

    /**
     * Reset statistics (for multi-run scenarios)
     */
    virtual void resetStatistics() {
        statistics_.clear();
    }

protected:
    PassConfig config_;
    std::unordered_map<std::string, int> statistics_;

    /**
     * Increment a statistic counter
     */
    void incrementStat(const std::string& name, int amount = 1) {
        statistics_[name] += amount;
    }

    /**
     * Check if transformation should be applied based on probability
     * Uses the pass's RNG for reproducibility
     */
    bool shouldTransform();

    /**
     * Check if a function should be processed based on include/exclude lists
     */
    bool shouldProcessFunction(const std::string& func_name) const {
        // If include list is specified, function must be in it
        if (!config_.include_functions.empty()) {
            bool found = false;
            for (const auto& f : config_.include_functions) {
                if (f == func_name) { found = true; break; }
            }
            if (!found) return false;
        }

        // Check exclude list
        for (const auto& f : config_.exclude_functions) {
            if (f == func_name) return false;
        }

        return true;
    }
};

/**
 * GIMPLE-specific transformation pass
 * For GCC plugin integration
 */
class GimpleTransformationPass : public TransformationPass {
public:
    PassType getPassType() const override { return PassType::Gimple; }

    /**
     * Transform a GIMPLE function
     *
     * @param func GCC function pointer (cast from void* for portability)
     * @return Result of transformation
     */
    virtual TransformResult transformGimple(void* func) = 0;
};

/**
 * LLVM IR-specific transformation pass
 * For standalone IR obfuscator
 */
class LLVMTransformationPass : public TransformationPass {
public:
    PassType getPassType() const override { return PassType::LLVM; }

    /**
     * Transform LLVM IR lines
     *
     * @param lines Vector of IR lines to transform
     * @return Result of transformation
     */
    virtual TransformResult transformIR(std::vector<std::string>& lines) = 0;
};

/**
 * Assembly-specific transformation pass
 * For assembly-level obfuscation
 */
class AssemblyTransformationPass : public TransformationPass {
public:
    PassType getPassType() const override { return PassType::Assembly; }

    /**
     * Transform assembly lines
     *
     * @param lines Vector of assembly lines to transform
     * @param arch Target architecture ("x86_64", "x86_32", "arm64", etc.)
     * @return Result of transformation
     */
    virtual TransformResult transformAssembly(
        std::vector<std::string>& lines,
        const std::string& arch
    ) = 0;
};

} // namespace morphect

#endif // MORPHECT_TRANSFORMATION_BASE_HPP
