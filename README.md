# Morphect

A compiler-level code obfuscation framework.

```
  __  __                  _               _
 |  \/  | ___  _ __ _ __ | |__   ___  ___| |_
 | |\/| |/ _ \| '__| '_ \| '_ \ / _ \/ __| __|
 | |  | | (_) | |  | |_) | | | |  __/ (__| |_
 |_|  |_|\___/|_|  | .__/|_| |_|\___|\___|\__|
                   |_|
```

---

## Overview

Morphect transforms compiled code into functionally equivalent but structurally complex output that resists reverse engineering. Operating at compiler intermediate representations (GCC GIMPLE and LLVM IR), it supports any language that compiles through these toolchains.

The framework provides multiple obfuscation techniques that can be combined based on protection requirements and performance constraints.

---

## Features

| Category | Techniques |
|----------|------------|
| Arithmetic | Mixed Boolean Arithmetic transforms simple operations into complex equivalents |
| Control Flow | Flattening converts structured code to state machines; bogus branches add fake paths |
| Data | String encryption, constant obfuscation, variable splitting |
| Anti-Analysis | Dead code insertion, anti-debugging, anti-disassembly |

---

## Quick Start

### Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### LLVM IR Workflow

```bash
# Generate IR
clang -S -emit-llvm -O0 source.c -o source.ll

# Obfuscate
./build/bin/morphect-ir --mba --cff source.ll obfuscated.ll

# Compile
clang obfuscated.ll -o program
```

### GCC Plugin

```bash
gcc -fplugin=./build/lib/morphect_plugin.so source.c -o program
```

### Assembly Obfuscator

```bash
# Generate Intel-syntax assembly
gcc -S -masm=intel source.c -o source.s

# Obfuscate
./build/bin/morphect-asm --probability 0.8 source.s obfuscated.s

# Assemble and link
gcc obfuscated.s -o program
```

---

## Command Line Options

### morphect-ir (LLVM IR)

```
morphect-ir [options] <input.ll> <output.ll>

Passes:
  --mba         Mixed Boolean Arithmetic
  --cff         Control Flow Flattening
  --bogus       Bogus Control Flow
  --varsplit    Variable Splitting
  --strenc      String Encoding
  --deadcode    Dead Code Insertion
  --all         Enable all passes

Settings:
  --config      JSON configuration file
  --probability Transformation probability (0.0-1.0)
  --verbose     Detailed output
```

### morphect-asm (Assembly)

```
morphect-asm [options] <input.s> <output.s>

Features:
  - Control flow obfuscation (opaque predicates, bogus branches)
  - MBA (Mixed Boolean-Arithmetic) transformations
  - Constant obfuscation
  - Dead code insertion
  - Label randomization

Settings:
  --config      JSON configuration file
  --probability Transformation probability (0.0-1.0)
  --verbose     Detailed output
```

---

## Configuration

Create a JSON configuration file for fine-grained control:

```json
{
  "global": {
    "probability": 0.85
  },
  "mba": {
    "enabled": true,
    "chain_depth": 2
  },
  "cff": {
    "enabled": true,
    "max_blocks": 50
  }
}
```

Use with:

```bash
morphect-ir --config config.json input.ll output.ll
```

---

## Language Support

Any language targeting GCC or LLVM:

| Language | Method |
|----------|--------|
| C, C++ | LLVM IR or GCC plugin |
| Rust | `rustc --emit=llvm-ir` |
| Zig | `zig build-obj -femit-llvm-ir` |
| Go | gccgo with plugin |
| Fortran | gfortran with plugin |

---

## Requirements

- CMake 3.20+
- C++17 compiler (GCC 12+ or Clang 14+)
- GCC plugin headers (for GIMPLE support)

---

## Performance Impact

| Metric | Typical Change |
|--------|----------------|
| Binary size | +10-30% |
| Runtime | +2-10% slower |
| Compile time | +10-30% |

Impact varies with configuration. Use probability settings and function exclusion to balance protection and performance.

---

## Documentation

Complete documentation is available in the manual:

| Document | Description |
|----------|-------------|
| [Introduction](manual/00-introduction.md) | Overview and design philosophy |
| [Building](manual/01-building.md) | Build requirements and procedures |
| [Quick Start](manual/02-quickstart.md) | Basic usage examples |
| [LLVM IR Tool](manual/03-llvm-ir.md) | morphect-ir reference |
| [GCC Plugin](manual/04-gcc-plugin.md) | Plugin usage and integration |
| [Assembly Obfuscator](manual/05-assembly.md) | morphect-asm reference |
| [Techniques](manual/06-techniques.md) | How obfuscation methods work |
| [Configuration](manual/07-configuration.md) | Complete configuration reference |
| [Architecture](manual/08-architecture.md) | Internal design and extension |
| [API Reference](manual/09-api.md) | Programmatic interface |
| [Troubleshooting](manual/10-troubleshooting.md) | Common issues and solutions |

---

## Project Structure

```
src/
  core/           Pass management and base classes
  common/         Logging, random, JSON parsing
  passes/
    mba/          Arithmetic obfuscation
    cff/          Control flow flattening
    control_flow/ Indirect branches and calls
    data/         Strings, constants, variables
    deadcode/     Dead code insertion
    antidisasm/   Anti-disassembly
    antidebug/    Debugger detection
  backends/
    gimple/       GCC plugin
    llvm_ir/      LLVM IR tool
    assembly/     Assembly tool
tests/            Unit and integration tests
manual/           Documentation
```

---

## License

Apache License, Version 2.0

---

## Disclaimer

This software is intended for protecting legitimate software against reverse engineering. Use responsibly and in accordance with applicable laws.
