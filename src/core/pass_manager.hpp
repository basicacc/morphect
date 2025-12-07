/*
 * pass_manager.hpp
 *
 * handles registering and running all the obfuscation passes
 */

#ifndef MORPHECT_PASS_MANAGER_HPP
#define MORPHECT_PASS_MANAGER_HPP

#include "transformation_base.hpp"
#include "statistics.hpp"
#include "../common/logging.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>

namespace morphect {

struct PassEntry {
    std::unique_ptr<TransformationPass> pass;
    bool enabled;
    int order;
};

class PassManager {
public:
    PassManager() : logger_("PassManager") {}

    // takes ownership of the pass
    template<typename T>
    void registerPass(std::unique_ptr<T> pass) {
        static_assert(
            std::is_base_of<TransformationPass, T>::value,
            "Pass must inherit from TransformationPass"
        );

        std::string name = pass->getName();

        if (passes_.find(name) != passes_.end()) {
            logger_.warn("Pass '{}' already registered, replacing", name);
        }

        PassEntry entry;
        entry.pass = std::move(pass);
        entry.enabled = true;
        entry.order = static_cast<int>(entry.pass->getPriority());

        passes_[name] = std::move(entry);
        pass_order_dirty_ = true;

        logger_.debug("Registered pass: {}", name);
    }

    TransformationPass* getPass(const std::string& name) {
        auto it = passes_.find(name);
        if (it != passes_.end()) {
            return it->second.pass.get();
        }
        return nullptr;
    }

    bool setPassEnabled(const std::string& name, bool enabled) {
        auto it = passes_.find(name);
        if (it != passes_.end()) {
            it->second.enabled = enabled;
            it->second.pass->setEnabled(enabled);
            return true;
        }
        return false;
    }

    void setPassOrder(const std::vector<std::string>& order) {
        custom_order_ = order;
        pass_order_dirty_ = true;
    }

    bool initialize(const PassConfig& config) {
        global_config_ = config;
        bool success = true;

        for (auto& [name, entry] : passes_) {
            if (!entry.pass->initialize(config)) {
                logger_.error("Failed to initialize pass: {}", name);
                success = false;
            } else {
                logger_.debug("Initialized pass: {}", name);
            }
        }

        computePassOrder();
        return success;
    }

    std::vector<std::string> getPassOrder() {
        if (pass_order_dirty_) {
            computePassOrder();
        }
        return ordered_passes_;
    }

    int runGimplePasses(void* func) {
        if (pass_order_dirty_) {
            computePassOrder();
        }

        int total_transforms = 0;

        for (const auto& name : ordered_passes_) {
            auto& entry = passes_[name];
            if (!entry.enabled) continue;

            // no RTTI in gcc plugins so we use static dispatch
            if (entry.pass->getPassType() == PassType::Gimple) {
                auto* gimple_pass = static_cast<GimpleTransformationPass*>(entry.pass.get());
                auto result = gimple_pass->transformGimple(func);
                if (result == TransformResult::Success) {
                    total_transforms++;
                }
            }
        }

        functions_processed_++;
        return total_transforms;
    }

    int runLLVMPasses(std::vector<std::string>& lines) {
        if (pass_order_dirty_) {
            computePassOrder();
        }

        int total_transforms = 0;

        for (const auto& name : ordered_passes_) {
            auto& entry = passes_[name];
            if (!entry.enabled) continue;

            if (entry.pass->getPassType() == PassType::LLVM) {
                auto* llvm_pass = static_cast<LLVMTransformationPass*>(entry.pass.get());
                auto result = llvm_pass->transformIR(lines);
                if (result == TransformResult::Success) {
                    total_transforms++;
                }
            }
        }

        functions_processed_++;
        return total_transforms;
    }

    int runAssemblyPasses(std::vector<std::string>& lines, const std::string& arch) {
        if (pass_order_dirty_) {
            computePassOrder();
        }

        int total_transforms = 0;

        for (const auto& name : ordered_passes_) {
            auto& entry = passes_[name];
            if (!entry.enabled) continue;

            if (entry.pass->getPassType() == PassType::Assembly) {
                auto* asm_pass = static_cast<AssemblyTransformationPass*>(entry.pass.get());
                auto result = asm_pass->transformAssembly(lines, arch);
                if (result == TransformResult::Success) {
                    total_transforms++;
                }
            }
        }

        functions_processed_++;
        return total_transforms;
    }

    void finalize() {
        for (auto& [name, entry] : passes_) {
            entry.pass->finalize();
        }
    }

    Statistics getStatistics() const {
        Statistics stats;

        stats.set("functions_processed", functions_processed_);
        stats.set("passes_registered", static_cast<int>(passes_.size()));

        for (const auto& [name, entry] : passes_) {
            auto pass_stats = entry.pass->getStatistics();
            for (const auto& [stat_name, value] : pass_stats) {
                stats.set(name + "." + stat_name, value);
            }
        }

        return stats;
    }

    void printStatistics() {
        logger_.info("=== Morphect Obfuscation Statistics ===");
        logger_.info("Functions processed: {}", functions_processed_);
        logger_.info("Passes registered: {}", passes_.size());
        logger_.info("");

        for (const auto& name : ordered_passes_) {
            auto& entry = passes_[name];
            auto stats = entry.pass->getStatistics();

            if (stats.empty()) continue;

            logger_.info("Pass: {} {}", name, entry.enabled ? "" : "(disabled)");
            for (const auto& [stat_name, value] : stats) {
                logger_.info("  {}: {}", stat_name, value);
            }
        }

        logger_.info("========================================");
    }

    void resetStatistics() {
        functions_processed_ = 0;
        for (auto& [name, entry] : passes_) {
            entry.pass->resetStatistics();
        }
    }

    std::vector<std::string> getRegisteredPasses() const {
        std::vector<std::string> names;
        for (const auto& [name, entry] : passes_) {
            names.push_back(name);
        }
        return names;
    }

private:
    std::unordered_map<std::string, PassEntry> passes_;
    std::vector<std::string> ordered_passes_;
    std::vector<std::string> custom_order_;
    bool pass_order_dirty_ = true;
    int functions_processed_ = 0;
    PassConfig global_config_;
    Logger logger_;

    void computePassOrder() {
        ordered_passes_.clear();

        if (!custom_order_.empty()) {
            for (const auto& name : custom_order_) {
                if (passes_.find(name) != passes_.end()) {
                    ordered_passes_.push_back(name);
                }
            }
            // add passes not in custom order at the end
            for (const auto& [name, entry] : passes_) {
                if (std::find(ordered_passes_.begin(), ordered_passes_.end(), name)
                    == ordered_passes_.end()) {
                    ordered_passes_.push_back(name);
                }
            }
        } else {
            std::vector<std::pair<std::string, int>> pass_priorities;
            for (const auto& [name, entry] : passes_) {
                pass_priorities.emplace_back(name, entry.order);
            }

            std::sort(pass_priorities.begin(), pass_priorities.end(),
                [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });

            for (const auto& [name, priority] : pass_priorities) {
                ordered_passes_.push_back(name);
            }
        }

        resolveDependencies();

        pass_order_dirty_ = false;

        logger_.debug("Pass order computed:");
        for (size_t i = 0; i < ordered_passes_.size(); i++) {
            logger_.debug("  {}: {}", i + 1, ordered_passes_[i]);
        }
    }

    // basic dependency resolution - keeps iterating until order is stable
    void resolveDependencies() {
        bool changed = true;
        int iterations = 0;
        const int max_iterations = 100;

        while (changed && iterations < max_iterations) {
            changed = false;
            iterations++;

            for (size_t i = 0; i < ordered_passes_.size(); i++) {
                const auto& name = ordered_passes_[i];
                auto& entry = passes_[name];
                auto deps = entry.pass->getDependencies();

                for (const auto& dep : deps) {
                    auto dep_it = std::find(ordered_passes_.begin(),
                                           ordered_passes_.end(), dep);

                    if (dep_it == ordered_passes_.end()) {
                        logger_.warn("Pass '{}' depends on '{}' which is not registered",
                                    name, dep);
                        continue;
                    }

                    size_t dep_pos = std::distance(ordered_passes_.begin(), dep_it);

                    if (dep_pos > i) {
                        ordered_passes_.erase(dep_it);
                        ordered_passes_.insert(ordered_passes_.begin() + i, dep);
                        changed = true;
                        break;
                    }
                }

                if (changed) break;
            }
        }

        if (iterations >= max_iterations) {
            logger_.error("Possible circular dependency in passes");
        }
    }
};

} // namespace morphect

#endif // MORPHECT_PASS_MANAGER_HPP
