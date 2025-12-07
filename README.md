# Morphect

A code obfuscator that actually works across multiple languages.

```
  __  __                  _               _
 |  \/  | ___  _ __ _ __ | |__   ___  ___| |_
 | |\/| |/ _ \| '__| '_ \| '_ \ / _ \/ __| __|
 | |  | | (_) | |  | |_) | | | |  __/ (__| |_
 |_|  |_|\___/|_|  | .__/|_| |_|\___|\___|\__|
                   |_|
```

## What is this?

Morphect transforms your code into something that does the same thing but looks completely different. It works at the compiler level (GCC plugin, LLVM IR) so you can use it with C, C++, Rust, Zig, and basically anything that compiles through GCC or LLVM.

The goal is to make reverse engineering painful without breaking your program.

## Features

**Arithmetic obfuscation (MBA)**
- Turns `a + b` into stuff like `(a ^ b) + 2 * (a & b)`
- Same result, way more confusing to analyze
- Works on +, -, *, ^, &, |

**Control flow mess**
- Flattening - turns your if/else into a giant switch-case state machine
- Fake branches with opaque predicates (conditions that always evaluate the same but look complex)
- Indirect jumps and calls through function pointers

**Data protection**
- String encryption (XOR, rolling XOR, RC4)
- Constants get split into computed expressions
- Variables can be split into multiple parts

**Anti-analysis**
- Junk bytes that confuse disassemblers
- Debugger detection (ptrace, timing checks, etc)
- Dead code insertion

## Quick start

### As a GCC plugin

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# then just add it to your gcc command
gcc -fplugin=./build/lib/morphect_plugin.so your_code.c -o output
```

### With LLVM IR

```bash
cmake -B build -DMORPHECT_BUILD_IR_OBFUSCATOR=ON
cmake --build build

clang -S -emit-llvm -O0 code.c -o code.ll
./build/bin/morphect-ir code.ll obfuscated.ll
llc obfuscated.ll -o obfuscated.s
gcc obfuscated.s -o output
```

## Configuration

You can control everything through a JSON config file:

```json
{
  "obfuscation_settings": {
    "global_probability": 0.85
  },
  "mba": {
    "enabled": true,
    "chain_depth": 3
  },
  "control_flow": {
    "cff_enabled": true,
    "bogus_cf_enabled": true
  },
  "data": {
    "string_encoding": {
      "enabled": true,
      "method": "xor"
    }
  }
}
```

Pass it like:
```bash
gcc -fplugin=./build/lib/morphect_plugin.so \
    -fplugin-arg-morphect_plugin-config=config.json \
    code.c -o output
```

## Language support

| Language | How | Notes |
|----------|-----|-------|
| C/C++ | GCC plugin or LLVM | works great |
| Rust | LLVM IR | emit-llvm then obfuscate |
| Zig | LLVM IR | same deal |
| Fortran | GCC plugin (gfortran) | yep |
| Go | gccgo | works |
| D | gdc or ldc | both work |

## Building

Requirements:
- GCC 12+ with plugin headers
- CMake 3.20+
- C++17 compiler

```bash
cmake -B build
cmake --build build -j$(nproc)

# run tests
ctest --test-dir build
```

## How it works

The basic idea is pretty simple - we hook into the compiler's intermediate representation and mess with it before it becomes machine code.

For example, here's what MBA does to addition:

```c
// you write
int result = a + b;

// compiler sees (after obfuscation)
int t1 = a ^ b;
int t2 = a & b;
int result = t1 + (t2 << 1);
```

Math checks out: `(a ^ b) + 2*(a & b) = a + b` for any integers.

Control flow flattening is more involved - it takes your structured code and turns it into a while loop with a switch inside, using a state variable to decide which block runs next. Looks awful in a decompiler.

## Performance impact

Real talk: this will make your code slower and bigger.

| What | How much |
|------|----------|
| Binary size | +8-20% |
| Runtime | -2-8% slower |
| Compile time | +10-30% |

Worth it if you need to protect your code. Not worth it for code that needs to be fast.

## What's next

Things I want to add:

- [ ] VM-based protection (like VMProtect but lighter)
- [ ] Better control flow analysis to avoid breaking exception handling
- [ ] Windows support for anti-debug (currently Linux-focused)
- [ ] Function merging/cloning
- [ ] Self-modifying code sections
- [ ] Better docs and examples
- [ ] Maybe a GUI config tool

If you want to help with any of this, PRs welcome.

## Project structure

```
src/
  core/           - pass manager, base classes
  common/         - logging, random, json parsing
  backends/       - GCC plugin, LLVM tool, asm tool
  passes/
    mba/          - arithmetic obfuscation
    cff/          - control flow flattening
    control_flow/ - indirect branches/calls
    data/         - strings, constants
    deadcode/     - dead code insertion
    antidisasm/   - anti-disassembly
    antidebug/    - debugger detection
tests/            - unit tests, integration tests
docs/             - detailed guides
```

## License

MIT. Do whatever you want with it.

## Disclaimer

This is for protecting legitimate software - think anti-piracy, protecting proprietary algorithms, etc. Don't use it for malware. That's not cool and probably illegal where you live.
