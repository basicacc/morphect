/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * antidisasm_base.hpp - Base definitions for anti-disassembly techniques
 *
 * Anti-disassembly techniques confuse disassemblers by:
 *   - Inserting junk bytes after unconditional jumps
 *   - Creating overlapping instructions
 *   - Inserting fake function prologues
 *   - Using opaque predicates with dead code
 *
 * These techniques target linear sweep disassemblers (like objdump)
 * and can also confuse recursive descent disassemblers (like IDA/Ghidra).
 */

#ifndef MORPHECT_ANTIDISASM_BASE_HPP
#define MORPHECT_ANTIDISASM_BASE_HPP

#include "../../core/transformation_base.hpp"
#include "../../common/random.hpp"
#include "../../common/logging.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_set>

namespace morphect {
namespace antidisasm {

/**
 * Types of anti-disassembly techniques
 */
enum class AntiDisasmTechnique {
    JunkBytes,              // Insert junk bytes after jumps
    InstructionOverlap,     // Overlapping instructions
    FakePrologue,           // Fake function prologues
    OpaqueJump,             // Opaque predicate with conditional jump
    DisassemblerBug,        // Target known disassembler bugs
    SelfModifying           // Self-modifying code patterns
};

/**
 * Target architecture for anti-disassembly
 */
enum class TargetArch {
    X86_32,
    X86_64,
    ARM32,
    ARM64,
    Unknown
};

/**
 * Junk byte patterns that look like valid instruction prefixes
 * These confuse linear sweep disassemblers
 */
struct JunkBytePattern {
    std::string name;
    std::vector<uint8_t> bytes;
    std::string description;
    bool is_prefix = false;  // If true, looks like instruction prefix
};

/**
 * Configuration for anti-disassembly
 */
struct AntiDisasmConfig {
    bool enabled = true;
    double probability = 0.5;           // Probability to apply at each opportunity
    TargetArch target_arch = TargetArch::X86_64;

    // Technique-specific settings
    bool insert_junk_bytes = true;
    bool use_instruction_overlap = true;
    bool insert_fake_prologues = true;
    bool use_opaque_jumps = true;

    // Junk byte settings
    int min_junk_bytes = 1;
    int max_junk_bytes = 4;
    bool prefer_prefix_bytes = true;    // Use bytes that look like prefixes

    // Instruction overlap settings
    bool use_simple_overlap = true;     // Simple jump+junk overlap
    bool use_complex_overlap = false;   // Multi-byte NOP overlap

    // Fake prologue settings
    int max_fake_prologues = 3;         // Max fake prologues per function
    bool vary_prologue_style = true;    // Use different prologue patterns

    // Safety
    bool preserve_alignment = true;     // Don't break alignment requirements
    std::vector<std::string> exclude_functions;
};

/**
 * Result of anti-disassembly transformation
 */
struct AntiDisasmResult {
    bool success = false;
    std::vector<std::string> transformed_code;
    int junk_bytes_inserted = 0;
    int overlaps_created = 0;
    int fake_prologues_inserted = 0;
    int opaque_jumps_inserted = 0;
};

// ============================================================================
// x86/x64 Junk Byte Patterns
// ============================================================================

/**
 * Collection of junk bytes for x86/x64
 * These bytes confuse disassemblers but won't execute (after unconditional jumps)
 */
class X86JunkBytes {
public:
    /**
     * Get random junk bytes that look like instruction prefixes
     */
    static std::vector<uint8_t> getPrefixLikeBytes(int count) {
        static const std::vector<uint8_t> prefix_bytes = {
            0x26,  // ES segment override
            0x2E,  // CS segment override / branch not taken hint
            0x36,  // SS segment override
            0x3E,  // DS segment override / branch taken hint
            0x64,  // FS segment override
            0x65,  // GS segment override
            0x66,  // Operand size override
            0x67,  // Address size override
            0xF0,  // LOCK prefix
            0xF2,  // REPNE/REPNZ prefix
            0xF3,  // REP/REPE/REPZ prefix
        };

        std::vector<uint8_t> result;
        for (int i = 0; i < count; i++) {
            int idx = GlobalRandom::nextInt(0, static_cast<int>(prefix_bytes.size()) - 1);
            result.push_back(prefix_bytes[idx]);
        }
        return result;
    }

    /**
     * Get random junk bytes that look like partial instructions
     */
    static std::vector<uint8_t> getInstructionLikeBytes(int count) {
        static const std::vector<uint8_t> inst_bytes = {
            0x0F,  // Two-byte opcode escape
            0x48,  // REX.W prefix (x64)
            0x49,  // REX.WB prefix (x64)
            0x4C,  // REX.WR prefix (x64)
            0x89,  // MOV r/m, r (looks like mov start)
            0x8B,  // MOV r, r/m
            0xB8,  // MOV EAX, imm32
            0xC7,  // MOV r/m, imm
            0xE8,  // CALL rel32 (followed by random bytes looks like call)
            0xE9,  // JMP rel32
            0xFF,  // Various (inc, dec, call, jmp, push)
        };

        std::vector<uint8_t> result;
        for (int i = 0; i < count; i++) {
            int idx = GlobalRandom::nextInt(0, static_cast<int>(inst_bytes.size()) - 1);
            result.push_back(inst_bytes[idx]);
        }
        return result;
    }

    /**
     * Get bytes that form a fake CALL instruction
     * Confuses disassemblers into thinking there's a function call
     */
    static std::vector<uint8_t> getFakeCallBytes() {
        std::vector<uint8_t> result;
        result.push_back(0xE8);  // CALL rel32
        // Random 4-byte offset (will never execute)
        for (int i = 0; i < 4; i++) {
            result.push_back(static_cast<uint8_t>(GlobalRandom::nextInt(0, 255)));
        }
        return result;
    }

    /**
     * Get bytes for multi-byte NOP (used for alignment, can confuse disassemblers)
     */
    static std::vector<uint8_t> getMultiByteNop(int size) {
        // Intel recommended multi-byte NOPs
        static const std::vector<std::vector<uint8_t>> nops = {
            {0x90},                                     // 1: nop
            {0x66, 0x90},                               // 2: 66 nop
            {0x0F, 0x1F, 0x00},                         // 3: nop dword ptr [rax]
            {0x0F, 0x1F, 0x40, 0x00},                   // 4: nop dword ptr [rax+0]
            {0x0F, 0x1F, 0x44, 0x00, 0x00},             // 5: nop dword ptr [rax+rax*1+0]
            {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00},       // 6: 66 nop word ptr [rax+rax*1+0]
            {0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00}, // 7: nop dword ptr [rax+0]
            {0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}, // 8: nop dword ptr [rax+rax*1+0]
            {0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}, // 9: 66 nop word ptr...
        };

        if (size >= 1 && size <= 9) {
            return nops[size - 1];
        }

        // For larger sizes, combine NOPs
        std::vector<uint8_t> result;
        while (size > 0) {
            int chunk = std::min(size, 9);
            auto nop = nops[chunk - 1];
            result.insert(result.end(), nop.begin(), nop.end());
            size -= chunk;
        }
        return result;
    }

    /**
     * Convert bytes to assembly .byte directives
     */
    static std::string bytesToAsm(const std::vector<uint8_t>& bytes) {
        if (bytes.empty()) return "";

        std::string result = "    .byte ";
        for (size_t i = 0; i < bytes.size(); i++) {
            if (i > 0) result += ", ";
            result += "0x";
            const char* hex = "0123456789abcdef";
            result += hex[bytes[i] >> 4];
            result += hex[bytes[i] & 0xF];
        }
        return result;
    }
};

// ============================================================================
// Fake Function Prologues
// ============================================================================

/**
 * Generator for fake function prologues
 * These confuse function detection in disassemblers
 */
class FakePrologueGenerator {
public:
    /**
     * Get a fake x86-64 function prologue
     */
    static std::vector<std::string> getX64Prologue(int variant = -1) {
        static const std::vector<std::vector<std::string>> prologues = {
            // Standard prologue
            {
                "    push rbp",
                "    mov rbp, rsp",
            },
            // With stack frame
            {
                "    push rbp",
                "    mov rbp, rsp",
                "    sub rsp, 32",
            },
            // With callee-saved registers
            {
                "    push rbp",
                "    push rbx",
                "    push r12",
                "    mov rbp, rsp",
            },
            // Windows x64 style
            {
                "    push rbp",
                "    mov rbp, rsp",
                "    push rsi",
                "    push rdi",
            },
            // Minimal
            {
                "    push rbp",
            },
        };

        if (variant < 0 || variant >= static_cast<int>(prologues.size())) {
            variant = GlobalRandom::nextInt(0, static_cast<int>(prologues.size()) - 1);
        }
        return prologues[variant];
    }

    /**
     * Get a fake x86-32 function prologue
     */
    static std::vector<std::string> getX86Prologue(int variant = -1) {
        static const std::vector<std::vector<std::string>> prologues = {
            // Standard prologue
            {
                "    push ebp",
                "    mov ebp, esp",
            },
            // With stack frame
            {
                "    push ebp",
                "    mov ebp, esp",
                "    sub esp, 16",
            },
            // With callee-saved
            {
                "    push ebp",
                "    push ebx",
                "    push esi",
                "    push edi",
                "    mov ebp, esp",
            },
        };

        if (variant < 0 || variant >= static_cast<int>(prologues.size())) {
            variant = GlobalRandom::nextInt(0, static_cast<int>(prologues.size()) - 1);
        }
        return prologues[variant];
    }

    /**
     * Generate a complete fake function with prologue (never executed)
     */
    static std::vector<std::string> generateFakeFunction(
        const std::string& name, TargetArch arch) {

        std::vector<std::string> result;
        result.push_back("");
        result.push_back("    # Fake function (never executed, confuses disassemblers)");
        result.push_back(name + ":");

        auto prologue = (arch == TargetArch::X86_64) ?
            getX64Prologue() : getX86Prologue();
        result.insert(result.end(), prologue.begin(), prologue.end());

        // Add some fake body
        if (arch == TargetArch::X86_64) {
            result.push_back("    xor eax, eax");
            result.push_back("    pop rbp");
            result.push_back("    ret");
        } else {
            result.push_back("    xor eax, eax");
            result.push_back("    pop ebp");
            result.push_back("    ret");
        }

        return result;
    }
};

// ============================================================================
// Instruction Overlap Patterns
// ============================================================================

/**
 * Generator for instruction overlap patterns
 * These create code where a jump lands in the middle of another "instruction"
 */
class InstructionOverlapGenerator {
public:
    /**
     * Generate a simple overlap pattern for x86-64
     *
     * Pattern:
     *   jmp over
     *   .byte 0xE8  ; looks like start of CALL
     * over:
     *   ... real code ...
     *
     * Linear disassemblers will try to decode 0xE8 as CALL,
     * consuming the following bytes as the call target.
     */
    static std::vector<std::string> generateSimpleOverlap(
        const std::string& label_prefix) {

        std::vector<std::string> result;
        std::string over_label = label_prefix + "_over";

        result.push_back("    # Anti-disassembly: instruction overlap");
        result.push_back("    jmp " + over_label);

        // Junk bytes that look like instruction start
        auto junk = X86JunkBytes::getInstructionLikeBytes(
            GlobalRandom::nextInt(1, 3));
        result.push_back(X86JunkBytes::bytesToAsm(junk));

        result.push_back(over_label + ":");

        return result;
    }

    /**
     * Generate a more complex overlap using opaque predicate
     *
     * Pattern:
     *   xor eax, eax      ; eax = 0
     *   jz real_code      ; always taken (ZF=1)
     *   .byte 0x0F, 0x0B  ; UD2 (never executed, but disassembler sees it)
     * real_code:
     *   ...
     */
    static std::vector<std::string> generateOpaqueOverlap(
        const std::string& label_prefix, TargetArch arch) {

        std::vector<std::string> result;
        std::string real_label = label_prefix + "_real";

        result.push_back("    # Anti-disassembly: opaque predicate overlap");

        // Generate opaque predicate (always true/false)
        int choice = GlobalRandom::nextInt(0, 2);

        if (choice == 0) {
            // xor reg, reg sets ZF=1, so jz is always taken
            result.push_back("    xor eax, eax");
            result.push_back("    jz " + real_label);
        } else if (choice == 1) {
            // test reg, reg after setting to non-zero, jnz always taken
            result.push_back("    mov eax, 1");
            result.push_back("    test eax, eax");
            result.push_back("    jnz " + real_label);
        } else {
            // Compare equal values
            std::string reg = (arch == TargetArch::X86_64) ? "rax" : "eax";
            result.push_back("    push " + reg);
            result.push_back("    cmp " + reg + ", " + reg);
            result.push_back("    pop " + reg);
            result.push_back("    je " + real_label);
        }

        // Dead code that will never execute but confuses disassembler
        result.push_back("    .byte 0x0F, 0x0B  # UD2 (never reached)");

        result.push_back(real_label + ":");

        return result;
    }
};

// ============================================================================
// Anti-Disassembly Analyzer
// ============================================================================

/**
 * Analyzes assembly code to find insertion points
 */
class AssemblyAnalyzer {
public:
    /**
     * Find unconditional jump instructions where junk can be inserted
     */
    std::vector<int> findUnconditionalJumps(
        const std::vector<std::string>& lines) const {

        std::vector<int> result;

        for (size_t i = 0; i < lines.size(); i++) {
            std::string trimmed = trim(lines[i]);

            // Skip empty, comments, directives
            if (trimmed.empty() || trimmed[0] == '#' ||
                trimmed[0] == ';' || trimmed[0] == '.') {
                continue;
            }

            // Check for unconditional jumps
            if (trimmed.find("jmp ") == 0 ||
                trimmed.find("jmp\t") == 0 ||
                trimmed.find("    jmp ") != std::string::npos ||
                trimmed.find("\tjmp ") != std::string::npos) {
                result.push_back(static_cast<int>(i));
            }
        }

        return result;
    }

    /**
     * Find suitable locations for fake prologues
     * (Between functions, after data sections, etc.)
     */
    std::vector<int> findPrologueInsertPoints(
        const std::vector<std::string>& lines) const {

        std::vector<int> result;
        bool in_function = false;

        for (size_t i = 0; i < lines.size(); i++) {
            std::string trimmed = trim(lines[i]);

            // Track function boundaries
            if (trimmed.find(".type") != std::string::npos &&
                trimmed.find("@function") != std::string::npos) {
                in_function = true;
            }

            if (trimmed.find(".size") != std::string::npos) {
                // End of function - good place for fake prologue
                result.push_back(static_cast<int>(i));
                in_function = false;
            }

            // After .text directive
            if (trimmed == ".text") {
                result.push_back(static_cast<int>(i));
            }
        }

        return result;
    }

    /**
     * Find function entry points for overlap insertion
     */
    std::vector<int> findFunctionEntries(
        const std::vector<std::string>& lines) const {

        std::vector<int> result;

        for (size_t i = 0; i < lines.size(); i++) {
            std::string trimmed = trim(lines[i]);

            // Look for function labels followed by prologue
            if (!trimmed.empty() && trimmed.back() == ':' &&
                trimmed.find('.') == std::string::npos) {
                // Check next few lines for prologue pattern
                if (i + 1 < lines.size()) {
                    std::string next = trim(lines[i + 1]);
                    if (next.find("push") != std::string::npos) {
                        result.push_back(static_cast<int>(i + 1));
                    }
                }
            }
        }

        return result;
    }

    /**
     * Detect architecture from assembly
     */
    TargetArch detectArch(const std::vector<std::string>& lines) const {
        for (const auto& line : lines) {
            // x86-64 indicators
            if (line.find("rax") != std::string::npos ||
                line.find("rbx") != std::string::npos ||
                line.find("rsp") != std::string::npos ||
                line.find("rbp") != std::string::npos ||
                line.find("r8") != std::string::npos ||
                line.find("r15") != std::string::npos) {
                return TargetArch::X86_64;
            }
            // x86-32 indicators
            if (line.find("eax") != std::string::npos ||
                line.find("ebx") != std::string::npos ||
                line.find("esp") != std::string::npos ||
                line.find("ebp") != std::string::npos) {
                // Could still be x64 using 32-bit regs, check more
                continue;
            }
        }
        return TargetArch::X86_64;  // Default
    }

private:
    std::string trim(const std::string& s) const {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }
};

/**
 * Abstract base for anti-disassembly transformations
 */
class AntiDisasmTransformation {
public:
    virtual ~AntiDisasmTransformation() = default;

    /**
     * Get the name of this transformation
     */
    virtual std::string getName() const = 0;

    /**
     * Transform assembly code with anti-disassembly techniques
     *
     * @param lines The assembly lines to transform
     * @param config Configuration for transformation
     * @return Result of transformation
     */
    virtual AntiDisasmResult transform(
        const std::vector<std::string>& lines,
        const AntiDisasmConfig& config) = 0;

protected:
    static int label_counter_;

    /**
     * Generate unique label
     */
    std::string nextLabel(const std::string& prefix = "_antidis") {
        return prefix + std::to_string(label_counter_++);
    }
};

// Initialize static counter
inline int AntiDisasmTransformation::label_counter_ = 0;

} // namespace antidisasm
} // namespace morphect

#endif // MORPHECT_ANTIDISASM_BASE_HPP
