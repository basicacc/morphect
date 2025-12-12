# LLVM IR Obfuscator Reference

Complete documentation for the morphect-ir tool.

---

## Contents

1. [Overview](#overview)
2. [Command Line Interface](#command-line-interface)
3. [Obfuscation Passes](#obfuscation-passes)
4. [Configuration File](#configuration-file)
5. [Integration with Build Systems](#integration-with-build-systems)
6. [Multi-Language Support](#multi-language-support)
7. [Advanced Usage](#advanced-usage)

---

## Overview

The morphect-ir tool transforms LLVM intermediate representation files. It reads textual IR (`.ll` files), applies obfuscation passes, and outputs transformed IR that compiles to functionally identical but structurally different code.

```
Source Code          LLVM IR              Obfuscated IR         Executable
     |                  |                      |                    |
     v                  v                      v                    v
  [source.c] --clang--> [source.ll] --morphect-ir--> [obf.ll] --clang--> [program]
```

---

## Command Line Interface

### Synopsis

```
morphect-ir [options] <input.ll> <output.ll>
```

### Arguments

| Argument | Description |
|----------|-------------|
| `input.ll` | Input LLVM IR file |
| `output.ll` | Output file for obfuscated IR |

### Options

#### Obfuscation Passes

| Option | Description |
|--------|-------------|
| `--mba` | Enable Mixed Boolean Arithmetic transformations |
| `--cff` | Enable Control Flow Flattening |
| `--bogus` | Enable Bogus Control Flow insertion |
| `--varsplit` | Enable Variable Splitting |
| `--strenc` | Enable String Encoding |
| `--deadcode` | Enable Dead Code Insertion |
| `--all` | Enable all obfuscation passes |

#### Settings

| Option | Description |
|--------|-------------|
| `--config <file>` | Load configuration from JSON file |
| `--probability <n>` | Set global transformation probability (0.0-1.0) |
| `--verbose`, `-v` | Enable detailed output |
| `--help`, `-h` | Display help message |

### Examples

```bash
# Single pass
morphect-ir --mba input.ll output.ll

# Multiple passes
morphect-ir --mba --cff --bogus input.ll output.ll

# All passes with full probability
morphect-ir --all --probability 1.0 input.ll output.ll

# Using configuration file
morphect-ir --config obfuscation.json input.ll output.ll

# Verbose output
morphect-ir --mba --verbose input.ll output.ll
```

---

## Obfuscation Passes

### MBA (Mixed Boolean Arithmetic)

Transforms arithmetic and bitwise operations into mathematically equivalent but complex expressions.

**Transformations:**

| Original | Obfuscated Form |
|----------|-----------------|
| `a + b` | `(a ^ b) + 2 * (a & b)` |
| `a - b` | `a + (~b + 1)` |
| `a ^ b` | `(a \| b) - (a & b)` |
| `a & b` | `(a + b) - (a \| b)` |
| `a \| b` | `(a ^ b) + (a & b)` |
| `a * k` | Decomposed shifts and adds |

**Example:**

Original IR:
```llvm
%result = add i32 %a, %b
```

After MBA:
```llvm
%t1 = xor i32 %a, %b
%t2 = and i32 %a, %b
%t3 = shl i32 %t2, 1
%result = add i32 %t1, %t3
```

**Options:**

```json
{
  "mba": {
    "enabled": true,
    "probability": 0.85,
    "chain_depth": 2,
    "operations": ["add", "sub", "xor", "and", "or", "mul"]
  }
}
```

### CFF (Control Flow Flattening)

Converts structured control flow into a switch-based state machine.

**Effect:**

```
Before:                 After:

  if (x)                while (1) {
    A();                  switch (state) {
  else                      case 0: state = x ? 1 : 2; break;
    B();                    case 1: A(); state = 3; break;
  C();                      case 2: B(); state = 3; break;
                            case 3: C(); return;
                          }
                        }
```

**Options:**

```json
{
  "cff": {
    "enabled": true,
    "probability": 0.8,
    "min_blocks": 3,
    "max_blocks": 100,
    "shuffle_states": true
  }
}
```

### Bogus Control Flow

Inserts fake branches with opaque predicates that never execute.

**Effect:**

```llvm
; Before
%result = add i32 %a, %b

; After
%pred = icmp sge i32 %x, 0    ; Always true for x*x
%x_sq = mul i32 %x, %x
%pred = icmp sge i32 %x_sq, 0
br i1 %pred, label %real, label %fake

real:
  %result = add i32 %a, %b
  br label %continue

fake:                          ; Never executed
  %bogus = sub i32 %a, %b
  br label %continue

continue:
  ; ...
```

**Options:**

```json
{
  "bogus": {
    "enabled": true,
    "probability": 0.5,
    "generate_dead_code": true
  }
}
```

### Variable Splitting

Splits scalar variables into multiple components that combine at use sites.

**Effect:**

```llvm
; Before
%secret = load i32, i32* %ptr

; After
%part1 = load i32, i32* %ptr_part1
%part2 = load i32, i32* %ptr_part2
%secret = add i32 %part1, %part2
```

**Options:**

```json
{
  "varsplit": {
    "enabled": true,
    "probability": 0.5,
    "method": "additive"
  }
}
```

### String Encoding

Encodes string literals and generates runtime decoders.

**Methods:**

| Method | Description |
|--------|-------------|
| `xor` | XOR with single-byte key |
| `rolling_xor` | XOR with position-dependent key |
| `rc4` | RC4 stream cipher |

**Options:**

```json
{
  "strenc": {
    "enabled": true,
    "method": "xor",
    "min_length": 3
  }
}
```

Note: String encoding requires linking the morphect runtime library or implementing a decoder.

### Dead Code Insertion

Adds computations that look legitimate but do not affect program output.

**Options:**

```json
{
  "deadcode": {
    "enabled": true,
    "probability": 0.3,
    "complexity": "medium"
  }
}
```

---

## Configuration File

Configuration files provide fine-grained control over obfuscation behavior.

### Format

JSON format with hierarchical options:

```json
{
  "global": {
    "probability": 0.85,
    "verbose": false
  },
  "mba": {
    "enabled": true,
    "probability": 0.9,
    "chain_depth": 2
  },
  "cff": {
    "enabled": true,
    "probability": 0.8,
    "min_blocks": 3,
    "max_blocks": 50
  },
  "bogus": {
    "enabled": true,
    "probability": 0.5
  },
  "varsplit": {
    "enabled": false
  },
  "deadcode": {
    "enabled": true,
    "probability": 0.3
  }
}
```

### Usage

```bash
morphect-ir --config config.json input.ll output.ll
```

Command-line options override configuration file settings.

### Complete Reference

```json
{
  "global": {
    "probability": 0.85,
    "verbose": false,
    "seed": 0
  },
  "mba": {
    "enabled": true,
    "probability": 0.85,
    "chain_depth": 2,
    "operations": {
      "add": true,
      "sub": true,
      "xor": true,
      "and": true,
      "or": true,
      "mul": true
    }
  },
  "cff": {
    "enabled": true,
    "probability": 0.8,
    "min_blocks": 3,
    "max_blocks": 100,
    "shuffle_states": true,
    "state_var_name": "_cff_state"
  },
  "bogus": {
    "enabled": true,
    "probability": 0.5,
    "generate_dead_code": true,
    "dead_code_lines": 3
  },
  "varsplit": {
    "enabled": true,
    "probability": 0.5,
    "method": "additive",
    "max_splits": 10
  },
  "strenc": {
    "enabled": true,
    "method": "xor",
    "min_length": 3
  },
  "deadcode": {
    "enabled": true,
    "probability": 0.3,
    "complexity": "medium",
    "max_blocks": 5
  }
}
```

---

## Integration with Build Systems

### Makefile

```makefile
CC = clang
MORPHECT = ./morphect-ir
MORPHECT_FLAGS = --mba --cff

%.ll: %.c
	$(CC) -S -emit-llvm -O0 $< -o $@

%_obf.ll: %.ll
	$(MORPHECT) $(MORPHECT_FLAGS) $< $@

%: %_obf.ll
	$(CC) -O2 $< -o $@
```

### CMake

```cmake
# Custom function for obfuscation
function(add_obfuscated_executable target source)
    set(ll_file ${CMAKE_CURRENT_BINARY_DIR}/${target}.ll)
    set(obf_file ${CMAKE_CURRENT_BINARY_DIR}/${target}_obf.ll)

    # Generate IR
    add_custom_command(
        OUTPUT ${ll_file}
        COMMAND clang -S -emit-llvm -O0 ${source} -o ${ll_file}
        DEPENDS ${source}
    )

    # Obfuscate
    add_custom_command(
        OUTPUT ${obf_file}
        COMMAND morphect-ir --mba --cff ${ll_file} ${obf_file}
        DEPENDS ${ll_file}
    )

    # Compile
    add_custom_target(${target} ALL
        COMMAND clang -O2 ${obf_file} -o ${target}
        DEPENDS ${obf_file}
    )
endfunction()
```

### Shell Script

```bash
#!/bin/bash
# obfuscate_build.sh

set -e

SOURCE=$1
OUTPUT=${2:-${SOURCE%.c}}

TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Generate IR
clang -S -emit-llvm -O0 "$SOURCE" -o "$TEMP_DIR/input.ll"

# Obfuscate
morphect-ir --mba --cff "$TEMP_DIR/input.ll" "$TEMP_DIR/output.ll"

# Compile
clang -O2 "$TEMP_DIR/output.ll" -o "$OUTPUT"

echo "Built: $OUTPUT"
```

---

## Multi-Language Support

### Rust

```bash
# Generate LLVM IR
rustc --emit=llvm-ir -O source.rs -o source.ll

# Obfuscate
morphect-ir --mba --cff source.ll source_obf.ll

# Compile (requires rustc runtime)
clang source_obf.ll -L$(rustc --print sysroot)/lib -o program
```

### Zig

```bash
# Generate LLVM IR
zig build-obj -femit-llvm-ir source.zig

# Obfuscate
morphect-ir --mba source.ll source_obf.ll

# Compile
zig build-exe source_obf.ll
```

### C++

```bash
# Generate IR (note: C++ may require linking standard library)
clang++ -S -emit-llvm -O0 source.cpp -o source.ll

# Obfuscate
morphect-ir --mba --cff source.ll source_obf.ll

# Compile with C++ runtime
clang++ source_obf.ll -o program
```

---

## Advanced Usage

### Selective Obfuscation

Use configuration to exclude specific functions:

```json
{
  "global": {
    "exclude_functions": ["performance_critical", "timing_*"]
  }
}
```

### Deterministic Output

Set a seed for reproducible obfuscation:

```json
{
  "global": {
    "seed": 12345
  }
}
```

### Debugging Obfuscated Code

For debugging, generate debug information through the obfuscation process:

```bash
# Generate IR with debug info
clang -S -emit-llvm -g source.c -o source.ll

# Obfuscate (debug info preserved where possible)
morphect-ir --mba source.ll source_obf.ll

# Compile with debug info
clang -g source_obf.ll -o program
```

Note: Obfuscation significantly complicates debugging. Maintain a non-obfuscated build for development.

### Measuring Impact

Compare before and after:

```bash
# Sizes
wc -l input.ll output.ll
ls -la original obfuscated

# Performance
time ./original
time ./obfuscated
```

---

*Previous: [Quick Start Guide](02-quickstart.md) | Next: [GCC Plugin](04-gcc-plugin.md)*
