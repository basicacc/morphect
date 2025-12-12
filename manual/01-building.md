# Building and Installation

This chapter covers the requirements and procedures for building Morphect from source.

---

## Contents

1. [Requirements](#requirements)
2. [Basic Build](#basic-build)
3. [Build Options](#build-options)
4. [Verifying the Build](#verifying-the-build)
5. [Installation](#installation)
6. [Platform Notes](#platform-notes)

---

## Requirements

### Compiler

A C++17 compliant compiler is required:

- GCC 12 or later
- Clang 14 or later
- MSVC 2019 or later (Windows)

### Build System

- CMake 3.20 or later

### Optional Dependencies

| Component | Requirement | Purpose |
|-----------|-------------|---------|
| GCC Plugin | GCC 12+ with plugin headers | GIMPLE-based obfuscation |
| LLVM Tools | LLVM 14+ | Compiling obfuscated IR |
| GoogleTest | Fetched automatically | Running test suite |

### Checking Requirements

Verify your environment:

```bash
cmake --version    # Should be 3.20+
g++ --version      # Should be 12+
clang --version    # Should be 14+ (for LLVM workflow)
```

---

## Basic Build

The standard build procedure:

```bash
# Clone the repository
git clone <repository-url> morphect
cd morphect

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel
```

This produces:

| Output | Location | Description |
|--------|----------|-------------|
| morphect-ir | `build/bin/` | LLVM IR obfuscator |
| morphect-asm | `build/bin/` | Assembly obfuscator |
| morphect_plugin.so | `build/lib/` | GCC plugin (if enabled) |

---

## Build Options

CMake options control which components are built:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DMORPHECT_BUILD_GIMPLE_PLUGIN=ON \
    -DMORPHECT_BUILD_IR_OBFUSCATOR=ON \
    -DMORPHECT_BUILD_ASM_OBFUSCATOR=ON \
    -DMORPHECT_BUILD_TESTS=ON
```

### Available Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Debug | Build type (Debug, Release, RelWithDebInfo) |
| `MORPHECT_BUILD_GIMPLE_PLUGIN` | ON | Build GCC GIMPLE plugin |
| `MORPHECT_BUILD_IR_OBFUSCATOR` | ON | Build LLVM IR obfuscator |
| `MORPHECT_BUILD_ASM_OBFUSCATOR` | ON | Build assembly obfuscator |
| `MORPHECT_BUILD_TESTS` | OFF | Build test suite |
| `MORPHECT_BUILD_BENCHMARKS` | OFF | Build performance benchmarks |

### Build Types

**Debug**

Includes debug symbols, no optimization, assertions enabled. Suitable for development.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

**Release**

Full optimization, no debug symbols, assertions disabled. Suitable for production use.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

**RelWithDebInfo**

Optimization with debug symbols. Useful for profiling.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

---

## Verifying the Build

After building, verify the tools work correctly:

```bash
# Check LLVM IR obfuscator
./build/bin/morphect-ir --help

# Check assembly obfuscator
./build/bin/morphect-asm --help

# Check GCC plugin (if built)
ls -la ./build/lib/morphect_plugin.so
```

### Running Tests

If tests were enabled:

```bash
# Run all tests
ctest --test-dir build

# Run with verbose output
ctest --test-dir build --output-on-failure

# Run specific test suite
./build/bin/morphect_test --gtest_filter="MBATest.*"
```

### Quick Functionality Test

Verify the obfuscator transforms code correctly:

```bash
# Create test file
cat > /tmp/test.c << 'EOF'
int add(int a, int b) {
    return a + b;
}
int main() {
    return add(2, 3) - 5;
}
EOF

# Generate LLVM IR
clang -S -emit-llvm -O0 /tmp/test.c -o /tmp/test.ll

# Obfuscate
./build/bin/morphect-ir --mba /tmp/test.ll /tmp/test_obf.ll

# Compile and run
clang /tmp/test_obf.ll -o /tmp/test_obf
/tmp/test_obf
echo "Exit code: $?"  # Should be 0
```

---

## Installation

### System Installation

To install to a system location:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local

cmake --build build --parallel
sudo cmake --install build
```

This installs:

| Component | Location |
|-----------|----------|
| Binaries | `/usr/local/bin/` |
| Libraries | `/usr/local/lib/` |
| Headers | `/usr/local/include/morphect/` |
| CMake Config | `/usr/local/lib/cmake/morphect/` |

### Local Installation

For non-root installation:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local

cmake --build build --parallel
cmake --install build
```

Add to your PATH:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

---

## Platform Notes

### Linux

Standard build procedure applies. Ensure GCC plugin headers are installed for GIMPLE support:

```bash
# Debian/Ubuntu
sudo apt install gcc-12-plugin-dev

# Fedora
sudo dnf install gcc-plugin-devel

# Arch Linux
# Plugin headers included with gcc
```

### macOS

The GCC GIMPLE plugin may not build with Apple's toolchain. Use Homebrew GCC if GIMPLE support is needed:

```bash
brew install gcc

cmake -B build \
    -DMORPHECT_BUILD_GIMPLE_PLUGIN=OFF \
    -DMORPHECT_BUILD_IR_OBFUSCATOR=ON
```

The LLVM IR workflow is fully supported with Apple Clang.

### Windows

Build with Visual Studio 2019 or later:

```bash
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
```

Or with Ninja:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The GCC plugin is not available on Windows. Use the LLVM IR workflow.

---

## Troubleshooting Build Issues

### CMake Version Too Old

```
CMake Error at CMakeLists.txt:1 (cmake_minimum_required):
  CMake 3.20 or higher is required.
```

Update CMake:

```bash
# Using pip
pip install --upgrade cmake

# Or download from cmake.org
```

### Missing GCC Plugin Headers

```
Could not find GCC plugin headers
```

Install the plugin development package for your distribution, or disable the plugin:

```bash
cmake -B build -DMORPHECT_BUILD_GIMPLE_PLUGIN=OFF
```

### C++17 Not Supported

```
error: 'optional' is not a member of 'std'
```

Your compiler is too old. Update to GCC 12+ or Clang 14+.

---

*Previous: [Introduction](00-introduction.md) | Next: [Quick Start Guide](02-quickstart.md)*
