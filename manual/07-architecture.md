# Architecture

Internal design and structure of the Morphect framework.

---

## Contents

1. [Overview](#overview)
2. [Directory Structure](#directory-structure)
3. [Core Components](#core-components)
4. [Pass System](#pass-system)
5. [Backend Architecture](#backend-architecture)
6. [Data Flow](#data-flow)
7. [Extension Guide](#extension-guide)

---

## Overview

Morphect is designed as a modular, extensible framework with clear separation between:

- **Core infrastructure:** Pass management, configuration, utilities
- **Transformation passes:** Individual obfuscation techniques
- **Backends:** Compiler-specific integrations

```
+------------------------------------------------------------------+
|                        User Interface                             |
|                  (CLI tools, GCC plugin args)                     |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                     Configuration Layer                           |
|                  (JSON config, MorphectConfig)                    |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                        Pass Manager                               |
|          (Registration, Ordering, Dependency Resolution)          |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                    Transformation Passes                          |
|  +----------+  +----------+  +----------+  +----------+          |
|  |   MBA    |  |   CFF    |  |  Data    |  |  Anti-   |          |
|  |          |  |          |  |          |  | Analysis |          |
|  +----------+  +----------+  +----------+  +----------+          |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                       Backend Layer                               |
|  +------------+     +------------+     +----------------+        |
|  |   GIMPLE   |     |  LLVM IR   |     |   Assembly     |        |
|  | (GCC Plugin)|    | (Standalone)|    | (Post-compile) |        |
|  +------------+     +------------+     +----------------+        |
+------------------------------------------------------------------+
```

---

## Directory Structure

```
morphect/
|-- src/
|   |-- core/                    # Core infrastructure
|   |   |-- pass_manager.hpp     # Pass orchestration
|   |   |-- transformation_base.hpp  # Base classes
|   |   `-- statistics.hpp       # Metrics collection
|   |
|   |-- common/                  # Shared utilities
|   |   |-- logging.hpp          # Logging infrastructure
|   |   |-- random.hpp           # PRNG
|   |   `-- json_parser.hpp      # Configuration parsing
|   |
|   |-- passes/                  # Transformation implementations
|   |   |-- mba/                 # Mixed Boolean Arithmetic
|   |   |   |-- mba_add.cpp/hpp
|   |   |   |-- mba_sub.cpp/hpp
|   |   |   |-- mba_xor.cpp/hpp
|   |   |   |-- mba_and.cpp/hpp
|   |   |   |-- mba_or.cpp/hpp
|   |   |   `-- mba_mult.cpp/hpp
|   |   |
|   |   |-- cff/                 # Control Flow Flattening
|   |   |   |-- cff_base.hpp
|   |   |   |-- llvm_cfg_analyzer.cpp/hpp
|   |   |   |-- llvm_cff_transform.cpp/hpp
|   |   |   |-- opaque_predicates.hpp
|   |   |   `-- bogus_cf.hpp
|   |   |
|   |   |-- control_flow/        # Indirect control flow
|   |   |   |-- indirect_branch.cpp/hpp
|   |   |   |-- indirect_call.cpp/hpp
|   |   |   `-- call_stack_obf.cpp/hpp
|   |   |
|   |   |-- data/                # Data obfuscation
|   |   |   |-- string_encoding.cpp/hpp
|   |   |   |-- constant_obf.cpp/hpp
|   |   |   `-- variable_splitting.cpp/hpp
|   |   |
|   |   |-- deadcode/            # Dead code insertion
|   |   |   `-- dead_code.cpp/hpp
|   |   |
|   |   |-- antidisasm/          # Anti-disassembly
|   |   |   `-- antidisasm.cpp/hpp
|   |   |
|   |   `-- antidebug/           # Anti-debugging
|   |       `-- antidebug.cpp/hpp
|   |
|   |-- backends/                # Compiler integrations
|   |   |-- gimple/              # GCC plugin
|   |   |   `-- gimple_plugin.cpp
|   |   |
|   |   |-- llvm_ir/             # LLVM IR tool
|   |   |   `-- ir_obfuscator.cpp
|   |   |
|   |   `-- assembly/            # Assembly tool
|   |       `-- asm_obfuscator.cpp
|   |
|   `-- morphect.hpp             # Main header
|
|-- tests/
|   |-- unit/                    # Unit tests
|   |-- integration/             # Integration tests
|   |-- fixtures/                # Test infrastructure
|   `-- benchmarks/              # Performance tests
|
|-- manual/                      # Documentation
`-- CMakeLists.txt               # Build configuration
```

---

## Core Components

### PassManager

The PassManager orchestrates obfuscation passes:

```cpp
class PassManager {
public:
    // Registration
    template<typename PassType>
    bool registerPass(std::unique_ptr<PassType> pass);

    // Configuration
    void initialize(const MorphectConfig& config);
    void setPassEnabled(const std::string& name, bool enabled);

    // Execution
    int runLLVMPasses(std::vector<std::string>& ir_lines);
    int runGimplePasses(/* gimple context */);
    int runAssemblyPasses(std::vector<std::string>& asm_lines);

    // Information
    std::vector<std::string> getPassOrder() const;
    Statistics getStatistics() const;
};
```

### TransformationPass

Base class for all transformation passes:

```cpp
enum class TransformResult {
    Success,        // Applied successfully
    Skipped,        // Skipped (config/probability)
    NotApplicable,  // Doesn't apply
    Error           // Failed
};

enum class PassPriority {
    Early = 100,
    ControlFlow = 200,
    MBA = 400,
    Data = 600,
    Cleanup = 800,
    Late = 900
};

class TransformationPass {
public:
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual PassPriority getPriority() const = 0;
    virtual std::vector<std::string> getDependencies() const;

    virtual bool initialize(const PassConfig& config);

    // Statistics
    std::unordered_map<std::string, int> getStatistics() const;
    void incrementStat(const std::string& name, int delta = 1);
};
```

### MorphectConfig

Global configuration structure:

```cpp
struct MorphectConfig {
    // Global
    double global_probability = 0.85;
    bool preserve_functionality = true;
    int verbosity = 0;

    // Per-pass settings
    bool mba_enabled = true;
    int mba_chain_depth = 2;

    bool cff_enabled = true;
    int cff_min_blocks = 3;

    // ... additional settings

    static MorphectConfig fromFile(const std::string& path);
};
```

---

## Pass System

### Pass Registration

Passes self-register with metadata:

```cpp
class LLVMMBAPass : public LLVMTransformationPass {
public:
    std::string getName() const override {
        return "mba";
    }

    std::string getDescription() const override {
        return "Mixed Boolean Arithmetic transformations";
    }

    PassPriority getPriority() const override {
        return PassPriority::MBA;
    }

    std::vector<std::string> getDependencies() const override {
        return {};  // No dependencies
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        // Implementation
    }
};
```

### Pass Ordering

Passes execute in priority order:

| Priority | Value | Phase |
|----------|-------|-------|
| Early | 100 | Analysis, preparation |
| ControlFlow | 200 | CFF, bogus control flow |
| MBA | 400 | Arithmetic transformations |
| Data | 600 | String/constant obfuscation |
| Cleanup | 800 | Dead code, optimization |
| Late | 900 | Anti-analysis, finalization |

Dependencies are resolved before ordering:

```cpp
// If pass B depends on pass A, A runs first regardless of priority
std::vector<std::string> getDependencies() const override {
    return {"pass_a"};
}
```

### Pass Configuration

Each pass receives configuration:

```cpp
struct PassConfig {
    bool enabled = true;
    double probability = 1.0;
    int verbosity = 0;
    std::vector<std::string> include_functions;
    std::vector<std::string> exclude_functions;
};

bool initialize(const PassConfig& config) override {
    probability_ = config.probability;
    // ...
    return true;
}
```

---

## Backend Architecture

### LLVM IR Backend

Standalone tool processing textual IR:

```cpp
class LLVMIRObfuscator {
public:
    bool loadIR(const std::string& path);
    bool obfuscate(const MorphectConfig& config);
    bool writeIR(const std::string& path);

private:
    std::vector<std::string> ir_lines_;
    PassManager pass_manager_;
};
```

Workflow:
1. Parse IR file into lines
2. Identify function boundaries
3. Run passes on each function
4. Write transformed IR

### GCC GIMPLE Backend

GCC plugin hooking GIMPLE passes:

```cpp
// Plugin initialization
int plugin_init(struct plugin_name_args* info,
                struct plugin_gcc_version* version) {
    // Register GIMPLE pass
    register_callback(info->base_name,
                      PLUGIN_PASS_MANAGER_SETUP,
                      NULL,
                      &pass_info);
    return 0;
}

// Pass execution
unsigned int execute_morphect_pass(function* fun) {
    // Transform GIMPLE
    return 0;
}
```

### Assembly Backend

Post-compilation assembly transformation:

```cpp
class AssemblyObfuscator {
public:
    bool loadAssembly(const std::string& path);
    bool obfuscate(const MorphectConfig& config);
    bool writeAssembly(const std::string& path);

private:
    std::vector<std::string> asm_lines_;
    PassManager pass_manager_;
};
```

---

## Data Flow

### LLVM IR Obfuscation

```
1. Load IR file
       |
       v
2. Parse into functions
       |
       v
3. For each function:
   a. Check include/exclude filters
   b. Build CFG analysis
   c. Run passes in priority order:
      - CFF (if enabled)
      - MBA transformations
      - Variable splitting
      - Dead code insertion
   d. Collect statistics
       |
       v
4. Renumber SSA values
       |
       v
5. Write output IR
```

### GCC Plugin Flow

```
1. GCC frontend parses source
       |
       v
2. GIMPLE generation
       |
       v
3. Morphect pass executes:
   a. Iterate basic blocks
   b. Transform statements
   c. Update CFG
       |
       v
4. Continue GCC pipeline
       |
       v
5. Code generation
```

---

## Extension Guide

### Adding a New Pass

1. **Create header file:**

```cpp
// src/passes/mypass/my_pass.hpp
#pragma once
#include "core/transformation_base.hpp"

namespace morphect::mypass {

class MyPass : public LLVMTransformationPass {
public:
    std::string getName() const override;
    std::string getDescription() const override;
    PassPriority getPriority() const override;

    TransformResult transformIR(std::vector<std::string>& lines) override;

private:
    // Implementation details
};

}
```

2. **Implement the pass:**

```cpp
// src/passes/mypass/my_pass.cpp
#include "my_pass.hpp"

namespace morphect::mypass {

std::string MyPass::getName() const {
    return "my_pass";
}

std::string MyPass::getDescription() const {
    return "Description of my pass";
}

PassPriority MyPass::getPriority() const {
    return PassPriority::Data;  // Choose appropriate priority
}

TransformResult MyPass::transformIR(std::vector<std::string>& lines) {
    int transforms = 0;

    for (auto& line : lines) {
        if (shouldTransform(line)) {
            line = transform(line);
            transforms++;
        }
    }

    incrementStat("transformations", transforms);

    return transforms > 0 ? TransformResult::Success
                          : TransformResult::NotApplicable;
}

}
```

3. **Register in build system:**

Add to CMakeLists.txt:

```cmake
add_library(morphect_mypass STATIC
    src/passes/mypass/my_pass.cpp
)
```

4. **Register with PassManager:**

```cpp
pass_manager.registerPass(std::make_unique<MyPass>());
```

5. **Add configuration options:**

```cpp
struct MorphectConfig {
    // ...
    bool my_pass_enabled = true;
    double my_pass_probability = 0.5;
};
```

### Adding a New Backend

1. Create backend directory under `src/backends/`
2. Implement backend-specific pass base class
3. Create tool entry point
4. Add to CMakeLists.txt

---

## Thread Safety

- PassManager is single-threaded by design
- GlobalRandom uses thread-local storage
- Statistics collection is not thread-safe
- Run separate PassManager instances for parallelism

## Memory Management

- Passes owned by PassManager via unique_ptr
- IR/GIMPLE/ASM structures owned by compiler or caller
- Statistics use value semantics

## Error Handling

- Passes return TransformResult::Error on failure
- Errors logged but do not abort by default
- Configuration errors are fatal

---

*Previous: [Configuration Reference](06-configuration.md) | Next: [API Reference](08-api.md)*
