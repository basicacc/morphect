# Troubleshooting

Common issues and their solutions.

---

## Contents

1. [Build Issues](#build-issues)
2. [Runtime Issues](#runtime-issues)
3. [Obfuscation Issues](#obfuscation-issues)
4. [Performance Issues](#performance-issues)
5. [Debugging Obfuscated Code](#debugging-obfuscated-code)

---

## Build Issues

### CMake Version Too Old

**Error:**
```
CMake Error at CMakeLists.txt:1 (cmake_minimum_required):
  CMake 3.20 or higher is required.
```

**Solution:**

Update CMake:

```bash
# Using pip
pip install --upgrade cmake

# Or download from cmake.org
wget https://github.com/Kitware/CMake/releases/download/v3.28.0/cmake-3.28.0-linux-x86_64.sh
chmod +x cmake-3.28.0-linux-x86_64.sh
./cmake-3.28.0-linux-x86_64.sh --prefix=$HOME/.local
```

### C++17 Not Supported

**Error:**
```
error: 'optional' is not a member of 'std'
error: 'string_view' is not a member of 'std'
```

**Solution:**

Your compiler is too old. Update to GCC 12+ or Clang 14+:

```bash
# Ubuntu/Debian
sudo apt install g++-12

# Use it
cmake -B build -DCMAKE_CXX_COMPILER=g++-12
```

### Missing GCC Plugin Headers

**Error:**
```
Could not find GCC plugin headers
fatal error: gcc-plugin.h: No such file or directory
```

**Solution:**

Install plugin development package:

```bash
# Debian/Ubuntu
sudo apt install gcc-12-plugin-dev

# Fedora
sudo dnf install gcc-plugin-devel

# Arch
# Headers included with gcc package
```

Or disable the plugin:

```bash
cmake -B build -DMORPHECT_BUILD_GIMPLE_PLUGIN=OFF
```

### Linker Errors

**Error:**
```
undefined reference to `morphect::...'
```

**Solution:**

Ensure all libraries are linked:

```bash
# Check library order in CMakeLists.txt
target_link_libraries(morphect-ir
    morphect_mba
    morphect_cff
    morphect_data
    # ... all required libraries
)
```

---

## Runtime Issues

### morphect-ir: Command Not Found

**Solution:**

The tool is in the build directory:

```bash
./build/bin/morphect-ir --help

# Or add to PATH
export PATH="$PWD/build/bin:$PATH"
```

### Plugin Load Failure

**Error:**
```
cc1: error: cannot load plugin ./morphect_plugin.so
```

**Solution:**

1. Verify the file exists:
   ```bash
   ls -la ./build/lib/morphect_plugin.so
   ```

2. Use absolute path:
   ```bash
   gcc -fplugin=/full/path/to/morphect_plugin.so source.c
   ```

3. Check plugin compatibility:
   ```bash
   # Plugin must match GCC version
   gcc --version
   ```

### Invalid LLVM IR

**Error:**
```
error: expected instruction opcode
```

**Solution:**

1. Regenerate IR from source:
   ```bash
   clang -S -emit-llvm -O0 source.c -o source.ll
   ```

2. Verify IR is valid:
   ```bash
   llvm-as source.ll -o /dev/null
   ```

3. Check for manual edits that broke syntax.

---

## Obfuscation Issues

### Output Differs from Original

**Symptom:** Obfuscated program produces different output.

**Diagnosis:**

1. Test with minimal obfuscation:
   ```bash
   morphect-ir --mba --probability 0.1 input.ll test.ll
   clang test.ll -o test
   ./test
   ```

2. Binary search for problematic pass:
   ```bash
   # Test each pass individually
   morphect-ir --mba input.ll test_mba.ll
   morphect-ir --cff input.ll test_cff.ll
   # ...
   ```

3. Check for undefined behavior in original code.

**Common causes:**

- Uninitialized variables (undefined behavior exposed)
- Floating-point precision (obfuscation adds operations)
- Race conditions in multithreaded code

### Compilation Fails After Obfuscation

**Error:**
```
error: use of undefined value '%42'
```

**Solution:**

SSA value numbering issue. The obfuscator should renumber values. If this occurs:

1. Report the issue with the input IR
2. Try a different pass combination
3. Reduce probability to find triggering case

**Error:**
```
Instruction does not dominate all uses
```

**Solution:**

Control flow transformation created invalid IR. Typically caused by:

- Complex PHI node patterns
- Exception handling
- Computed goto

Try excluding the problematic function:

```json
{
  "global": {
    "exclude_functions": ["problematic_function"]
  }
}
```

### CFF Breaks Exception Handling

**Symptom:** C++ exceptions not caught after CFF.

**Solution:**

CFF preserves exception handling but complex patterns may fail. Options:

1. Exclude functions with exceptions:
   ```json
   {
     "cff": {
       "enabled": true
     },
     "global": {
       "exclude_functions": ["*catch*", "*throw*"]
     }
   }
   ```

2. Disable CFF for exception-heavy code.

### String Encoding Runtime Error

**Symptom:** Program crashes accessing encoded strings.

**Solution:**

String encoding requires runtime decoder. Either:

1. Link the morphect runtime library
2. Implement decoder in your code
3. Disable string encoding:
   ```bash
   morphect-ir --mba --cff input.ll output.ll  # No --strenc
   ```

---

## Performance Issues

### Compilation Too Slow

**Cause:** Complex transformations on large functions.

**Solutions:**

1. Reduce probability:
   ```bash
   morphect-ir --mba --probability 0.5 input.ll output.ll
   ```

2. Limit function size for CFF:
   ```json
   {
     "cff": {
       "max_blocks": 30
     }
   }
   ```

3. Exclude large functions:
   ```json
   {
     "global": {
       "exclude_functions": ["large_function"]
     }
   }
   ```

### Runtime Too Slow

**Cause:** Obfuscation adds overhead.

**Solutions:**

1. Profile to identify hotspots:
   ```bash
   perf record ./program
   perf report
   ```

2. Exclude hot functions:
   ```json
   {
     "global": {
       "exclude_functions": ["hot_inner_loop"]
     }
   }
   ```

3. Reduce MBA chain depth:
   ```json
   {
     "mba": {
       "chain_depth": 1
     }
   }
   ```

4. Disable CFF (highest overhead):
   ```json
   {
     "cff": {
       "enabled": false
     }
   }
   ```

### Binary Too Large

**Cause:** Code expansion from obfuscation.

**Solutions:**

1. Reduce probability settings
2. Disable dead code insertion
3. Reduce MBA chain depth
4. Enable compiler optimizations:
   ```bash
   clang -O2 obfuscated.ll -o program
   ```

---

## Debugging Obfuscated Code

### General Approach

Debugging obfuscated code is difficult by design. Maintain both:

- Development build (no obfuscation)
- Release build (obfuscated)

Debug issues in the development build first.

### Debug Symbols

Preserve debug information through obfuscation:

```bash
# Generate IR with debug info
clang -S -emit-llvm -g source.c -o source.ll

# Obfuscate
morphect-ir --mba source.ll obfuscated.ll

# Compile with debug info
clang -g obfuscated.ll -o program
```

Note: Debug info will reference transformed code, not original source.

### Logging

Add logging to narrow down issues:

```c
#include <stdio.h>
#define DEBUG_LOG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

void function() {
    DEBUG_LOG("Entering function");
    // ... code ...
    DEBUG_LOG("After critical section, x=%d", x);
}
```

### Reproducing Issues

Create minimal reproduction:

1. Extract problematic function
2. Create standalone test
3. Verify issue reproduces
4. Report with IR and configuration

### Verbose Mode

Enable verbose output to trace transformations:

```bash
morphect-ir --mba --verbose input.ll output.ll
```

Output shows:
- Functions processed
- Transformations applied
- Statistics

---

## Getting Help

### Information to Include

When reporting issues, provide:

1. Morphect version and build configuration
2. Compiler version (`gcc --version` or `clang --version`)
3. Operating system and version
4. Input IR (minimized if possible)
5. Configuration file used
6. Complete error message
7. Steps to reproduce

### Minimizing Test Cases

Reduce IR to smallest failing case:

```bash
# Try removing functions
# Try simplifying remaining function
# Identify minimal configuration that triggers issue
```

### Filing Issues

Report issues at the project repository with:

- Clear title describing the problem
- All information listed above
- Expected vs actual behavior

---

*Previous: [API Reference](08-api.md)*
