# GCC Plugin Reference

Complete documentation for the Morphect GCC plugin.

---

## Contents

1. [Overview](#overview)
2. [Plugin Loading](#plugin-loading)
3. [Plugin Arguments](#plugin-arguments)
4. [Integration Examples](#integration-examples)
5. [Supported Languages](#supported-languages)
6. [Limitations](#limitations)

---

## Overview

The Morphect GCC plugin integrates directly into the GCC compilation pipeline, transforming GIMPLE intermediate representation during compilation. This approach requires no separate obfuscation step and works with any GCC frontend.

```
Source Code --> GCC Frontend --> GIMPLE --> [Morphect Plugin] --> GIMPLE --> Backend --> Object
```

The plugin operates after GCC converts source code to GIMPLE (GCC's intermediate representation) but before code generation, allowing it to transform all operations at a high level of abstraction.

---

## Plugin Loading

### Basic Usage

Load the plugin with the `-fplugin` flag:

```bash
gcc -fplugin=./morphect_plugin.so source.c -o program
```

The plugin file must be accessible at the specified path. Use an absolute path to avoid issues:

```bash
gcc -fplugin=/full/path/to/morphect_plugin.so source.c -o program
```

### Verifying Plugin Loading

To verify the plugin loads correctly:

```bash
gcc -fplugin=./morphect_plugin.so --version
```

The plugin should produce no errors. Add `-v` for verbose output if troubleshooting.

---

## Plugin Arguments

Pass arguments to the plugin using `-fplugin-arg-morphect_plugin-<name>=<value>`:

### Available Arguments

| Argument | Values | Default | Description |
|----------|--------|---------|-------------|
| `config` | File path | None | JSON configuration file |
| `probability` | 0.0-1.0 | 0.85 | Transformation probability |
| `verbose` | (flag) | Off | Enable verbose output |

### Examples

**Set Probability:**

```bash
gcc -fplugin=./morphect_plugin.so \
    -fplugin-arg-morphect_plugin-probability=1.0 \
    source.c -o program
```

**Use Configuration File:**

```bash
gcc -fplugin=./morphect_plugin.so \
    -fplugin-arg-morphect_plugin-config=config.json \
    source.c -o program
```

**Enable Verbose Output:**

```bash
gcc -fplugin=./morphect_plugin.so \
    -fplugin-arg-morphect_plugin-verbose \
    source.c -o program
```

**Combined Arguments:**

```bash
gcc -fplugin=./morphect_plugin.so \
    -fplugin-arg-morphect_plugin-config=config.json \
    -fplugin-arg-morphect_plugin-probability=0.9 \
    -fplugin-arg-morphect_plugin-verbose \
    source.c -o program
```

---

## Integration Examples

### Simple Compilation

```bash
gcc -O2 -fplugin=./morphect_plugin.so source.c -o program
```

### With Configuration

Create a configuration file:

```json
{
  "mba": {
    "enabled": true,
    "probability": 0.8
  },
  "cff": {
    "enabled": true,
    "probability": 0.6
  }
}
```

Compile:

```bash
gcc -O2 \
    -fplugin=./morphect_plugin.so \
    -fplugin-arg-morphect_plugin-config=config.json \
    source.c -o program
```

### Makefile Integration

```makefile
CC = gcc
CFLAGS = -O2 -Wall
MORPHECT_PLUGIN = /path/to/morphect_plugin.so
MORPHECT_CONFIG = config.json

# Obfuscation flags
OBFUSCATE_FLAGS = -fplugin=$(MORPHECT_PLUGIN) \
                  -fplugin-arg-morphect_plugin-config=$(MORPHECT_CONFIG)

# Normal build
program: source.c
	$(CC) $(CFLAGS) $< -o $@

# Obfuscated build
program_obf: source.c
	$(CC) $(CFLAGS) $(OBFUSCATE_FLAGS) $< -o $@

# Build both
all: program program_obf

.PHONY: all
```

### CMake Integration

```cmake
# Find plugin
set(MORPHECT_PLUGIN "${CMAKE_SOURCE_DIR}/lib/morphect_plugin.so")

# Define obfuscation options
set(MORPHECT_COMPILE_OPTIONS
    -fplugin=${MORPHECT_PLUGIN}
    -fplugin-arg-morphect_plugin-probability=0.8
)

# Normal target
add_executable(program source.c)

# Obfuscated target
add_executable(program_obf source.c)
target_compile_options(program_obf PRIVATE ${MORPHECT_COMPILE_OPTIONS})
```

### Autotools Integration

In `configure.ac`:

```autoconf
AC_ARG_ENABLE([obfuscation],
    [AS_HELP_STRING([--enable-obfuscation], [Enable code obfuscation])],
    [enable_obfuscation=$enableval],
    [enable_obfuscation=no])

if test "x$enable_obfuscation" = "xyes"; then
    CFLAGS="$CFLAGS -fplugin=\$(top_srcdir)/morphect_plugin.so"
fi
```

---

## Supported Languages

The GCC plugin works with any language supported by a GCC frontend:

### C

```bash
gcc -fplugin=./morphect_plugin.so source.c -o program
```

### C++

```bash
g++ -fplugin=./morphect_plugin.so source.cpp -o program
```

### Fortran

```bash
gfortran -fplugin=./morphect_plugin.so source.f90 -o program
```

### Go (via gccgo)

```bash
gccgo -fplugin=./morphect_plugin.so source.go -o program
```

### D (via gdc)

```bash
gdc -fplugin=./morphect_plugin.so source.d -o program
```

---

## Limitations

### Platform Support

The GCC plugin is only available on systems where GCC plugins are supported:

| Platform | Support |
|----------|---------|
| Linux | Full support |
| macOS | Requires Homebrew GCC |
| Windows | Not supported |

### GCC Version

The plugin requires GCC 12 or later. Earlier versions have incompatible plugin APIs.

Check your GCC version:

```bash
gcc --version
```

### Plugin Headers

The plugin must be compiled against the same GCC version used for compilation. If you upgrade GCC, rebuild the plugin.

### Optimization Interaction

Some obfuscation transformations may interact with GCC optimizations:

- High optimization levels (`-O3`) may partially undo obfuscation
- Link-time optimization (`-flto`) may affect obfuscation effectiveness
- Profile-guided optimization may need adjustment

Recommendation: Test with your specific optimization flags to verify obfuscation effectiveness.

### Exception Handling

C++ exception handling is preserved but may interact with control flow obfuscation. Test exception paths thoroughly.

### Debugging

Obfuscated code is difficult to debug. The plugin preserves debug information where possible, but:

- Variable names may not match source
- Control flow will not match source
- Stepping through code will be confusing

Maintain a non-obfuscated build for development and debugging.

---

## Troubleshooting

### Plugin Not Found

```
gcc: error: ./morphect_plugin.so: No such file or directory
```

Verify the plugin path and ensure the file exists.

### Plugin Version Mismatch

```
cc1: error: plugin ./morphect_plugin.so is not licensed under a GPL-compatible license
```

This indicates a plugin ABI mismatch. Rebuild the plugin with your current GCC version.

### Missing Plugin Headers

If building the plugin fails due to missing headers:

```bash
# Debian/Ubuntu
sudo apt install gcc-12-plugin-dev

# Fedora
sudo dnf install gcc-plugin-devel
```

### Performance Issues

If compilation is slow with the plugin:

1. Reduce probability settings
2. Disable expensive passes (CFF)
3. Exclude large functions via configuration

---

*Previous: [LLVM IR Obfuscator](03-llvm-ir.md) | Next: [Assembly Obfuscator](05-assembly.md)*
