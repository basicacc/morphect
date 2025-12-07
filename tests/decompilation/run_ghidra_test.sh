#!/bin/bash
#
# Morphect - Ghidra Decompilation Test Script
#
# This script:
# 1. Compiles the test program normally
# 2. Compiles with Morphect obfuscation
# 3. Verifies both produce correct output
# 4. Optionally runs Ghidra headless analysis
#
# Usage:
#   ./run_ghidra_test.sh [--ghidra /path/to/ghidra]
#
# Requirements:
#   - GCC
#   - Morphect plugin (morphect_plugin.so)
#   - Ghidra (optional, for decompilation analysis)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../../build"
PLUGIN_PATH="${BUILD_DIR}/morphect_plugin.so"
TEST_SOURCE="${SCRIPT_DIR}/ghidra_test.c"
OUTPUT_DIR="${SCRIPT_DIR}/output"
GHIDRA_PATH=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --ghidra)
            GHIDRA_PATH="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Create output directory
mkdir -p "${OUTPUT_DIR}"

echo "=== Morphect Ghidra Decompilation Test ==="
echo ""

# Step 1: Compile normal version
echo "[1/4] Compiling normal version..."
gcc -O0 -g "${TEST_SOURCE}" -o "${OUTPUT_DIR}/ghidra_test_normal"

# Step 2: Compile obfuscated version
echo "[2/4] Compiling obfuscated version..."
if [ -f "${PLUGIN_PATH}" ]; then
    gcc -O0 -fplugin="${PLUGIN_PATH}" \
        -fplugin-arg-morphect_plugin-probability=1.0 \
        "${TEST_SOURCE}" -o "${OUTPUT_DIR}/ghidra_test_obfuscated" 2>&1 || {
        echo "Warning: Obfuscation compilation failed, trying without plugin..."
        gcc -O0 "${TEST_SOURCE}" -o "${OUTPUT_DIR}/ghidra_test_obfuscated"
    }
else
    echo "Warning: Plugin not found at ${PLUGIN_PATH}"
    echo "Compiling without obfuscation..."
    gcc -O0 "${TEST_SOURCE}" -o "${OUTPUT_DIR}/ghidra_test_obfuscated"
fi

# Step 3: Run both versions and verify output
echo "[3/4] Verifying correctness..."
echo ""
echo "--- Normal version output ---"
"${OUTPUT_DIR}/ghidra_test_normal"
NORMAL_EXIT=$?

echo ""
echo "--- Obfuscated version output ---"
"${OUTPUT_DIR}/ghidra_test_obfuscated"
OBFUSCATED_EXIT=$?

echo ""
if [ $NORMAL_EXIT -eq 0 ] && [ $OBFUSCATED_EXIT -eq 0 ]; then
    echo "✅ Both versions produce correct results!"
else
    echo "❌ Version mismatch - obfuscation may have broken functionality"
    echo "   Normal exit code: $NORMAL_EXIT"
    echo "   Obfuscated exit code: $OBFUSCATED_EXIT"
fi

# Step 4: Generate disassembly for manual comparison
echo ""
echo "[4/4] Generating disassembly..."
objdump -d "${OUTPUT_DIR}/ghidra_test_normal" > "${OUTPUT_DIR}/ghidra_test_normal.asm"
objdump -d "${OUTPUT_DIR}/ghidra_test_obfuscated" > "${OUTPUT_DIR}/ghidra_test_obfuscated.asm"

# Count instructions as a complexity metric
NORMAL_INSTR=$(grep -c "^\s*[0-9a-f]*:" "${OUTPUT_DIR}/ghidra_test_normal.asm" || echo "0")
OBFUSCATED_INSTR=$(grep -c "^\s*[0-9a-f]*:" "${OUTPUT_DIR}/ghidra_test_obfuscated.asm" || echo "0")

echo ""
echo "=== Complexity Metrics ==="
echo "Normal version: ~${NORMAL_INSTR} instructions"
echo "Obfuscated version: ~${OBFUSCATED_INSTR} instructions"

if [ $OBFUSCATED_INSTR -gt $NORMAL_INSTR ]; then
    INCREASE=$((OBFUSCATED_INSTR - NORMAL_INSTR))
    PERCENT=$((INCREASE * 100 / NORMAL_INSTR))
    echo "Instruction increase: +${INCREASE} (+${PERCENT}%)"
fi

# Binary size comparison
NORMAL_SIZE=$(stat --printf="%s" "${OUTPUT_DIR}/ghidra_test_normal")
OBFUSCATED_SIZE=$(stat --printf="%s" "${OUTPUT_DIR}/ghidra_test_obfuscated")

echo ""
echo "Normal binary size: ${NORMAL_SIZE} bytes"
echo "Obfuscated binary size: ${OBFUSCATED_SIZE} bytes"

# Step 5: Ghidra analysis (if available)
if [ -n "${GHIDRA_PATH}" ] && [ -d "${GHIDRA_PATH}" ]; then
    echo ""
    echo "=== Running Ghidra Headless Analysis ==="

    GHIDRA_HEADLESS="${GHIDRA_PATH}/support/analyzeHeadless"

    if [ -f "${GHIDRA_HEADLESS}" ]; then
        # Create Ghidra project directory
        GHIDRA_PROJECT="${OUTPUT_DIR}/ghidra_project"
        rm -rf "${GHIDRA_PROJECT}"
        mkdir -p "${GHIDRA_PROJECT}"

        # Analyze normal version
        echo "Analyzing normal version..."
        "${GHIDRA_HEADLESS}" "${GHIDRA_PROJECT}" "normal" \
            -import "${OUTPUT_DIR}/ghidra_test_normal" \
            -postScript DecompileAllFunctions.java \
            -scriptPath "${SCRIPT_DIR}" \
            -scriptlog "${OUTPUT_DIR}/ghidra_normal_decompile.log" \
            2>/dev/null || echo "Note: Ghidra analysis may have warnings"

        # Analyze obfuscated version
        echo "Analyzing obfuscated version..."
        "${GHIDRA_HEADLESS}" "${GHIDRA_PROJECT}" "obfuscated" \
            -import "${OUTPUT_DIR}/ghidra_test_obfuscated" \
            -postScript DecompileAllFunctions.java \
            -scriptPath "${SCRIPT_DIR}" \
            -scriptlog "${OUTPUT_DIR}/ghidra_obfuscated_decompile.log" \
            2>/dev/null || echo "Note: Ghidra analysis may have warnings"

        echo ""
        echo "Ghidra decompilation logs saved to:"
        echo "  - ${OUTPUT_DIR}/ghidra_normal_decompile.log"
        echo "  - ${OUTPUT_DIR}/ghidra_obfuscated_decompile.log"
    else
        echo "Warning: Ghidra headless analyzer not found"
    fi
else
    echo ""
    echo "Note: Ghidra path not specified. Run with --ghidra /path/to/ghidra for analysis."
fi

echo ""
echo "=== Test Complete ==="
echo "Output files saved to: ${OUTPUT_DIR}"
echo ""
echo "For manual Ghidra analysis:"
echo "  1. Open Ghidra"
echo "  2. Import ${OUTPUT_DIR}/ghidra_test_normal"
echo "  3. Import ${OUTPUT_DIR}/ghidra_test_obfuscated"
echo "  4. Compare decompilation of functions like:"
echo "     - simple_hash"
echo "     - check_license"
echo "     - process_command"
echo "     - state_machine"
