# Morphect

## A Multi-Language Code Obfuscation Framework

```
  __  __                  _               _
 |  \/  | ___  _ __ _ __ | |__   ___  ___| |_
 | |\/| |/ _ \| '__| '_ \| '_ \ / _ \/ __| __|
 | |  | | (_) | |  | |_) | | | |  __/ (__| |_
 |_|  |_|\___/|_|  | .__/|_| |_|\___|\___|\__|
                   |_|
```

Version 1.0

---

## Abstract

Morphect is a compiler-level code obfuscation framework designed to protect software against reverse engineering. Operating on intermediate representations (GCC GIMPLE and LLVM IR), it transforms source code into semantically equivalent but structurally complex output that resists static and dynamic analysis.

The framework supports multiple programming languages through its compiler integration approach: any language that compiles through GCC or LLVM can be obfuscated without source-level modifications.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Design Philosophy](#design-philosophy)
3. [Supported Languages](#supported-languages)
4. [Documentation Overview](#documentation-overview)

---

## Introduction

Code obfuscation transforms a program into a version that is functionally identical but significantly harder to understand through reverse engineering. Unlike encryption, obfuscated code remains executable without a decryption step, making it suitable for protecting software distributed in binary form.

Morphect implements obfuscation at the compiler intermediate representation level. This approach offers several advantages over source-to-source transformation:

- Language independence through standard compiler toolchains
- Access to type information and control flow structure
- Integration with existing build systems
- Preservation of compiler optimizations

The framework provides multiple categories of transformation:

| Category | Purpose |
|----------|---------|
| Arithmetic Obfuscation | Replaces simple operations with complex equivalents |
| Control Flow Obfuscation | Restructures program logic to hide execution paths |
| Data Obfuscation | Protects constants, strings, and variable relationships |
| Anti-Analysis | Impedes debuggers and disassemblers |

---

## Design Philosophy

Morphect follows several guiding principles:

**Correctness First**

Every transformation preserves program semantics. The obfuscated program produces identical output to the original for all inputs. Extensive testing validates each transformation against mathematical identities and runtime behavior.

**Modularity**

Obfuscation passes operate independently and can be combined as needed. Each pass has configurable probability and scope, allowing fine-grained control over the protection/performance tradeoff.

**Transparency**

The framework integrates with standard toolchains without requiring changes to source code. Developers compile normally, adding only plugin flags or an additional transformation step.

**Measurable Impact**

All transformations report statistics on their application. Performance and size impacts are documented, enabling informed decisions about protection levels.

---

## Supported Languages

Through compiler integration, Morphect supports any language targeting GCC or LLVM:

| Language | Backend | Integration Method |
|----------|---------|-------------------|
| C | GCC, LLVM | Native support |
| C++ | GCC, LLVM | Native support |
| Rust | LLVM | Via `--emit=llvm-ir` |
| Zig | LLVM | Via `-femit-llvm-ir` |
| Go | GCC | Via gccgo |
| Fortran | GCC | Via gfortran |
| D | GCC, LLVM | Via gdc or ldc |

---

## Documentation Overview

This manual is organized into the following sections:

| Document | Description |
|----------|-------------|
| [Building and Installation](01-building.md) | Compilation requirements and procedures |
| [Quick Start Guide](02-quickstart.md) | Basic usage examples |
| [LLVM IR Obfuscator](03-llvm-ir.md) | The morphect-ir tool reference |
| [GCC Plugin](04-gcc-plugin.md) | GIMPLE-based obfuscation |
| [Assembly Obfuscator](05-assembly.md) | Post-compilation assembly obfuscation |
| [Obfuscation Techniques](06-techniques.md) | Detailed explanation of transformations |
| [Configuration Reference](07-configuration.md) | JSON configuration options |
| [Architecture](08-architecture.md) | Internal design and extension |
| [API Reference](09-api.md) | Programmatic interface |
| [Troubleshooting](10-troubleshooting.md) | Common issues and solutions |

---

## License

Morphect is released under the Apache License, Version 2.0.

---

## Obtaining Morphect

Source code is available at the project repository. Binary releases are not currently provided; compilation from source is required.

---

*Continue to [Building and Installation](01-building.md)*
