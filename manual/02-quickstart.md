# Quick Start Guide

This chapter provides a practical introduction to using Morphect for code obfuscation.

---

## Contents

1. [Overview](#overview)
2. [LLVM IR Workflow](#llvm-ir-workflow)
3. [GCC Plugin Workflow](#gcc-plugin-workflow)
4. [Choosing Obfuscation Passes](#choosing-obfuscation-passes)
5. [Verifying Results](#verifying-results)

---

## Overview

Morphect provides two primary methods for obfuscating code:

| Method | Best For | Languages |
|--------|----------|-----------|
| LLVM IR | Cross-language, Clang-based projects | C, C++, Rust, Zig, Swift |
| GCC Plugin | GCC-based projects, no intermediate files | C, C++, Fortran, Go, D |

Both methods produce semantically identical executables with obfuscated internals.

---

## LLVM IR Workflow

The LLVM IR workflow operates on textual intermediate representation. This is the recommended approach for most use cases.

### Step 1: Compile to LLVM IR

Generate IR from your source code:

```bash
clang -S -emit-llvm -O0 source.c -o source.ll
```

Flags:
- `-S`: Output assembly format (textual IR)
- `-emit-llvm`: Produce LLVM IR instead of native assembly
- `-O0`: Disable optimizations (preserves structure for obfuscation)

### Step 2: Apply Obfuscation

Transform the IR with morphect-ir:

```bash
./build/bin/morphect-ir --mba --cff source.ll obfuscated.ll
```

Common pass combinations:

```bash
# Light obfuscation (arithmetic only)
morphect-ir --mba source.ll output.ll

# Medium obfuscation (arithmetic + control flow)
morphect-ir --mba --cff source.ll output.ll

# Heavy obfuscation (all passes)
morphect-ir --mba --cff --bogus --varsplit --deadcode source.ll output.ll
```

### Step 3: Compile to Executable

Produce the final binary:

```bash
clang obfuscated.ll -o program
```

For optimized output:

```bash
clang -O2 obfuscated.ll -o program
```

### Complete Example

```bash
# Source file
cat > example.c << 'EOF'
#include <stdio.h>

int compute(int x, int y) {
    if (x > y) {
        return x - y;
    } else {
        return x + y;
    }
}

int main() {
    int result = compute(10, 5);
    printf("Result: %d\n", result);
    return 0;
}
EOF

# Build pipeline
clang -S -emit-llvm -O0 example.c -o example.ll
./build/bin/morphect-ir --mba --cff example.ll example_obf.ll
clang example_obf.ll -o example_obf

# Execute
./example_obf
# Output: Result: 5
```

---

## GCC Plugin Workflow

The GCC plugin integrates directly into the compilation process, requiring no intermediate files.

### Basic Usage

Add the plugin to your GCC invocation:

```bash
gcc -O2 -fplugin=./build/lib/morphect_plugin.so source.c -o program
```

### With Configuration

Pass options through plugin arguments:

```bash
gcc -O2 \
    -fplugin=./build/lib/morphect_plugin.so \
    -fplugin-arg-morphect_plugin-config=config.json \
    source.c -o program
```

### Build System Integration

**Makefile**

```makefile
CC = gcc
CFLAGS = -O2 -fplugin=./morphect_plugin.so
PLUGIN_ARGS = -fplugin-arg-morphect_plugin-probability=0.8

program: source.c
    $(CC) $(CFLAGS) $(PLUGIN_ARGS) $< -o $@
```

**CMake**

```cmake
add_executable(program source.c)
target_compile_options(program PRIVATE
    -fplugin=${MORPHECT_PLUGIN_PATH}
    -fplugin-arg-morphect_plugin-config=${CONFIG_PATH}
)
```

---

## Choosing Obfuscation Passes

Select passes based on protection needs and performance budget:

### Pass Reference

| Pass | Flag | Effect | Performance Impact |
|------|------|--------|-------------------|
| MBA | `--mba` | Complex arithmetic | Low |
| CFF | `--cff` | Flattened control flow | Medium |
| Bogus | `--bogus` | Fake branches | Low |
| Variable Split | `--varsplit` | Split variables | Low |
| Dead Code | `--deadcode` | Unreachable code | Low |
| String Encode | `--strenc` | Encrypted strings | Medium |

### Recommended Configurations

**Minimal Protection**

For performance-critical code with basic protection:

```bash
morphect-ir --mba --probability 0.5 input.ll output.ll
```

**Standard Protection**

Balanced security and performance:

```bash
morphect-ir --mba --cff --bogus input.ll output.ll
```

**Maximum Protection**

For sensitive algorithms where performance is secondary:

```bash
morphect-ir --mba --cff --bogus --varsplit --deadcode --probability 1.0 input.ll output.ll
```

### Probability Control

The `--probability` flag controls how often transformations apply:

```bash
# Transform 50% of eligible operations
morphect-ir --mba --probability 0.5 input.ll output.ll

# Transform all eligible operations
morphect-ir --mba --probability 1.0 input.ll output.ll
```

Lower probability reduces protection but improves performance.

---

## Verifying Results

Always verify that obfuscation preserves program behavior.

### Functional Verification

Compare outputs of original and obfuscated versions:

```bash
# Compile both versions
clang source.ll -o original
clang obfuscated.ll -o obfuscated

# Compare outputs
./original > original_output.txt
./obfuscated > obfuscated_output.txt
diff original_output.txt obfuscated_output.txt
```

### Examining Transformations

Use verbose mode to see what changed:

```bash
morphect-ir --mba --verbose input.ll output.ll
```

Output shows:
- Functions analyzed
- Transformations applied
- Statistics per pass

### Inspecting IR Changes

Compare the IR before and after:

```bash
# Line count comparison
wc -l input.ll output.ll

# Visual diff (for small files)
diff input.ll output.ll | head -50
```

Obfuscated IR should be larger and contain different instruction patterns.

### Disassembly Comparison

For deeper analysis, compare disassembly:

```bash
# Compile both
clang -O2 input.ll -o original
clang -O2 output.ll -o obfuscated

# Disassemble
objdump -d original > original.asm
objdump -d obfuscated > obfuscated.asm

# Compare size
ls -la original obfuscated
```

---

## Next Steps

For detailed information on specific topics:

- [LLVM IR Obfuscator Reference](03-llvm-ir.md) - Complete morphect-ir documentation
- [GCC Plugin Reference](04-gcc-plugin.md) - Complete plugin documentation
- [Obfuscation Techniques](05-techniques.md) - How each transformation works
- [Configuration Reference](06-configuration.md) - JSON configuration format

---

*Previous: [Building and Installation](01-building.md) | Next: [LLVM IR Obfuscator](03-llvm-ir.md)*
