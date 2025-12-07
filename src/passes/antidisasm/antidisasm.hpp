/**
 * Morphect - Anti-Disassembly Transformations for x86/x64 Assembly
 *
 * Implements techniques to confuse disassemblers:
 *   - Junk bytes after unconditional jumps
 *   - Instruction overlap patterns
 *   - Fake function prologues
 *   - Opaque predicate jumps
 *
 * Example transformations:
 *
 * Junk bytes after JMP:
 *   jmp target
 *   .byte 0xE8, 0x12, 0x34, 0x56, 0x78  ; looks like CALL (never executed)
 *   target:
 *
 * Instruction overlap:
 *   jmp over
 *   .byte 0x0F  ; start of 2-byte opcode
 *   over:
 *   nop         ; disassembler may decode wrong instruction
 *
 * Fake prologue:
 *   .fake_func:
 *   push rbp
 *   mov rbp, rsp
 *   ; ... (never reached, confuses function detection)
 */

#ifndef MORPHECT_ANTIDISASM_HPP
#define MORPHECT_ANTIDISASM_HPP

#include "antidisasm_base.hpp"

#include <regex>
#include <sstream>
#include <algorithm>

namespace morphect {
namespace antidisasm {

/**
 * x86/x64 Assembly Anti-Disassembly Transformation
 */
class X86AntiDisasmTransformation : public AntiDisasmTransformation {
public:
    std::string getName() const override { return "X86_AntiDisasm"; }

    AntiDisasmResult transform(
        const std::vector<std::string>& lines,
        const AntiDisasmConfig& config) override {

        AntiDisasmResult result;
        result.transformed_code = lines;

        if (!config.enabled) {
            result.success = true;
            return result;
        }

        // Detect architecture
        AssemblyAnalyzer analyzer;
        TargetArch arch = analyzer.detectArch(lines);

        // Find insertion points
        auto jump_points = analyzer.findUnconditionalJumps(lines);
        auto prologue_points = analyzer.findPrologueInsertPoints(lines);
        auto function_entries = analyzer.findFunctionEntries(lines);

        // Track insertions (reverse order for safe modification)
        std::vector<std::pair<int, std::vector<std::string>>> insertions;

        // 1. Insert junk bytes after unconditional jumps
        if (config.insert_junk_bytes) {
            for (int point : jump_points) {
                if (GlobalRandom::nextDouble() > config.probability) {
                    continue;
                }

                auto junk = generateJunkBytes(config);
                insertions.push_back({point + 1, junk});
                result.junk_bytes_inserted++;
            }
        }

        // 2. Insert instruction overlap patterns
        if (config.use_instruction_overlap) {
            // Select some function entries for overlap
            int max_overlaps = std::min(3, static_cast<int>(function_entries.size()));
            for (int i = 0; i < max_overlaps; i++) {
                if (GlobalRandom::nextDouble() > config.probability) {
                    continue;
                }

                int point = function_entries[GlobalRandom::nextInt(0,
                    static_cast<int>(function_entries.size()) - 1)];

                std::vector<std::string> overlap;
                if (config.use_simple_overlap) {
                    overlap = InstructionOverlapGenerator::generateSimpleOverlap(
                        nextLabel());
                } else {
                    overlap = InstructionOverlapGenerator::generateOpaqueOverlap(
                        nextLabel(), arch);
                }

                insertions.push_back({point, overlap});
                result.overlaps_created++;
            }
        }

        // 3. Insert fake function prologues
        if (config.insert_fake_prologues && !prologue_points.empty()) {
            int num_prologues = GlobalRandom::nextInt(1,
                std::min(config.max_fake_prologues,
                        static_cast<int>(prologue_points.size())));

            for (int i = 0; i < num_prologues; i++) {
                if (GlobalRandom::nextDouble() > config.probability) {
                    continue;
                }

                int point = prologue_points[GlobalRandom::nextInt(0,
                    static_cast<int>(prologue_points.size()) - 1)];

                std::string fake_name = ".Lfake_" +
                    std::to_string(GlobalRandom::nextInt(1000, 9999));

                auto fake_func = FakePrologueGenerator::generateFakeFunction(
                    fake_name, arch);

                insertions.push_back({point + 1, fake_func});
                result.fake_prologues_inserted++;
            }
        }

        // 4. Add opaque jumps at strategic locations
        if (config.use_opaque_jumps) {
            for (int entry : function_entries) {
                if (GlobalRandom::nextDouble() > config.probability * 0.5) {
                    continue;
                }

                auto opaque = InstructionOverlapGenerator::generateOpaqueOverlap(
                    nextLabel(), arch);

                insertions.push_back({entry, opaque});
                result.opaque_jumps_inserted++;
            }
        }

        // Sort insertions by position (descending) for safe insertion
        std::sort(insertions.begin(), insertions.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        // Apply insertions
        for (const auto& [pos, code] : insertions) {
            if (pos >= 0 && pos <= static_cast<int>(result.transformed_code.size())) {
                result.transformed_code.insert(
                    result.transformed_code.begin() + pos,
                    code.begin(), code.end());
            }
        }

        result.success = true;
        return result;
    }

private:
    /**
     * Generate junk bytes based on configuration
     */
    std::vector<std::string> generateJunkBytes(const AntiDisasmConfig& config) {
        std::vector<std::string> result;

        int count = GlobalRandom::nextInt(config.min_junk_bytes,
                                          config.max_junk_bytes);

        result.push_back("    # Anti-disassembly: junk bytes");

        std::vector<uint8_t> bytes;
        if (config.prefer_prefix_bytes) {
            bytes = X86JunkBytes::getPrefixLikeBytes(count);
        } else {
            bytes = X86JunkBytes::getInstructionLikeBytes(count);
        }

        result.push_back(X86JunkBytes::bytesToAsm(bytes));

        return result;
    }
};

/**
 * Assembly Anti-Disassembly Pass
 */
class AssemblyAntiDisasmPass : public AssemblyTransformationPass {
public:
    AssemblyAntiDisasmPass() : transformer_() {}

    std::string getName() const override { return "AntiDisasm"; }
    std::string getDescription() const override {
        return "Inserts anti-disassembly techniques into assembly";
    }

    PassPriority getPriority() const override { return PassPriority::Late; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        ad_config_.enabled = config.enabled;
        ad_config_.probability = config.probability;
        return true;
    }

    void setAntiDisasmConfig(const AntiDisasmConfig& config) {
        ad_config_ = config;
    }

    const AntiDisasmConfig& getAntiDisasmConfig() const {
        return ad_config_;
    }

    TransformResult transformAssembly(
        std::vector<std::string>& lines,
        const std::string& arch) override {

        if (!ad_config_.enabled) {
            return TransformResult::Skipped;
        }

        // Set architecture based on parameter
        if (arch == "x86_64" || arch == "x64") {
            ad_config_.target_arch = TargetArch::X86_64;
        } else if (arch == "x86_32" || arch == "x86" || arch == "i386") {
            ad_config_.target_arch = TargetArch::X86_32;
        }

        auto ad_result = transformer_.transform(lines, ad_config_);

        if (ad_result.success) {
            lines = std::move(ad_result.transformed_code);
            statistics_["junk_bytes_inserted"] = ad_result.junk_bytes_inserted;
            statistics_["overlaps_created"] = ad_result.overlaps_created;
            statistics_["fake_prologues_inserted"] = ad_result.fake_prologues_inserted;
            statistics_["opaque_jumps_inserted"] = ad_result.opaque_jumps_inserted;
            return TransformResult::Success;
        } else {
            return TransformResult::Error;
        }
    }

    std::unordered_map<std::string, int> getStatistics() const override {
        return statistics_;
    }

private:
    X86AntiDisasmTransformation transformer_;
    AntiDisasmConfig ad_config_;
};

} // namespace antidisasm
} // namespace morphect

#endif // MORPHECT_ANTIDISASM_HPP
