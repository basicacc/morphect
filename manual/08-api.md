# API Reference

Programmatic interface for the Morphect framework.

---

## Contents

1. [Core API](#core-api)
2. [MBA API](#mba-api)
3. [Control Flow API](#control-flow-api)
4. [Data Obfuscation API](#data-obfuscation-api)
5. [Utility API](#utility-api)

---

## Core API

### PassManager

```cpp
#include "core/pass_manager.hpp"

namespace morphect {

class PassManager {
public:
    PassManager();
    ~PassManager();

    // Registration
    template<typename PassType>
    bool registerPass(std::unique_ptr<PassType> pass);

    // Configuration
    void initialize(const MorphectConfig& config);
    void setPassEnabled(const std::string& name, bool enabled);
    void setGlobalProbability(double probability);

    // Execution
    int runLLVMPasses(std::vector<std::string>& ir_lines);
    int runAssemblyPasses(std::vector<std::string>& asm_lines);

    // Information
    std::vector<std::string> getRegisteredPasses() const;
    std::vector<std::string> getPassOrder() const;
    bool isPassEnabled(const std::string& name) const;

    // Statistics
    Statistics getStatistics() const;
    void resetStatistics();
};

}
```

**Usage:**

```cpp
morphect::PassManager pm;

// Register passes
pm.registerPass(std::make_unique<morphect::mba::LLVMMBAPass>());
pm.registerPass(std::make_unique<morphect::cff::LLVMCFFPass>());

// Configure
morphect::MorphectConfig config;
config.global_probability = 0.9;
pm.initialize(config);

// Transform
std::vector<std::string> ir = loadIR("input.ll");
int count = pm.runLLVMPasses(ir);
writeIR("output.ll", ir);

// Report
auto stats = pm.getStatistics();
std::cout << "Transformations: " << stats.getInt("total") << std::endl;
```

### TransformationPass

```cpp
#include "core/transformation_base.hpp"

namespace morphect {

enum class TransformResult {
    Success,
    Skipped,
    NotApplicable,
    Error
};

enum class PassPriority {
    Early = 100,
    ControlFlow = 200,
    MBA = 400,
    Data = 600,
    Cleanup = 800,
    Late = 900
};

struct PassConfig {
    bool enabled = true;
    double probability = 1.0;
    int verbosity = 0;
    std::vector<std::string> include_functions;
    std::vector<std::string> exclude_functions;
};

class TransformationPass {
public:
    virtual ~TransformationPass() = default;

    // Identity
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual PassPriority getPriority() const = 0;
    virtual std::vector<std::string> getDependencies() const;

    // Lifecycle
    virtual bool initialize(const PassConfig& config);
    virtual void finalize();

    // Statistics
    std::unordered_map<std::string, int> getStatistics() const;

protected:
    void incrementStat(const std::string& name, int delta = 1);
    bool shouldProcess(double probability) const;
};

// LLVM IR specialization
class LLVMTransformationPass : public TransformationPass {
public:
    virtual TransformResult transformIR(std::vector<std::string>& lines) = 0;
};

// Assembly specialization
class AssemblyTransformationPass : public TransformationPass {
public:
    virtual TransformResult transformAsm(std::vector<std::string>& lines) = 0;
};

}
```

### MorphectConfig

```cpp
#include "morphect.hpp"

namespace morphect {

struct MorphectConfig {
    // Global settings
    double global_probability = 0.85;
    bool preserve_functionality = true;
    int verbosity = 0;
    uint64_t seed = 0;

    // MBA settings
    bool mba_enabled = true;
    double mba_probability = 0.85;
    int mba_chain_depth = 2;

    // CFF settings
    bool cff_enabled = true;
    double cff_probability = 0.8;
    int cff_min_blocks = 3;
    int cff_max_blocks = 100;

    // Bogus CF settings
    bool bogus_enabled = true;
    double bogus_probability = 0.5;

    // Variable splitting
    bool varsplit_enabled = true;
    double varsplit_probability = 0.5;

    // String encoding
    bool strenc_enabled = true;
    std::string strenc_method = "xor";

    // Dead code
    bool deadcode_enabled = true;
    double deadcode_probability = 0.3;

    // Factory methods
    static MorphectConfig fromFile(const std::string& path);
    static MorphectConfig defaults();
};

}
```

### Statistics

```cpp
#include "core/statistics.hpp"

namespace morphect {

class Statistics {
public:
    Statistics();

    // Setting values
    void set(const std::string& key, int value);
    void set(const std::string& key, const std::string& value);
    void increment(const std::string& key, int delta = 1);

    // Getting values
    int getInt(const std::string& key, int default_value = 0) const;
    std::string getString(const std::string& key,
                          const std::string& default_value = "") const;
    bool has(const std::string& key) const;

    // Merging
    void merge(const Statistics& other);

    // Output
    std::string toString() const;
    void print(std::ostream& os) const;
};

}
```

---

## MBA API

### MBAAdd

```cpp
#include "passes/mba/mba_add.hpp"

namespace morphect::mba {

struct MBAAddConfig {
    bool enabled = true;
    double probability = 0.85;
    int chain_depth = 2;
    int variant = -1;  // -1 = random
};

class MBAAdd {
public:
    MBAAdd();
    explicit MBAAdd(const MBAAddConfig& config);

    // Single transformation
    std::string transform(const std::string& a,
                          const std::string& b,
                          const std::string& type = "i32");

    // Chained transformation
    std::string transformChained(const std::string& a,
                                 const std::string& b,
                                 const std::string& type,
                                 int depth);

    // Line-level transformation
    std::string transformLine(const std::string& line);

    // Available variants
    static int getVariantCount();
    std::string getVariantDescription(int variant) const;
};

}
```

**Variants:**

| Variant | Formula |
|---------|---------|
| 0 | `(a ^ b) + 2 * (a & b)` |
| 1 | `(a \| b) + (a & b)` |
| 2 | `a - (~b) - 1` |
| 3 | `~(~a - b)` |
| 4 | `(a & ~b) + (b << 1) - (b & ~a)` |
| 5 | `((a ^ b) \| (a & b)) + (a & b)` |

### MBASub, MBAXor, MBAAnd, MBAOr

Similar interface to MBAAdd:

```cpp
namespace morphect::mba {

class MBASub {
public:
    std::string transform(const std::string& a, const std::string& b,
                          const std::string& type = "i32");
    std::string transformLine(const std::string& line);
};

class MBAXor {
public:
    std::string transform(const std::string& a, const std::string& b,
                          const std::string& type = "i32");
    std::string transformLine(const std::string& line);
};

class MBAAnd {
public:
    std::string transform(const std::string& a, const std::string& b,
                          const std::string& type = "i32");
    std::string transformLine(const std::string& line);
};

class MBAOr {
public:
    std::string transform(const std::string& a, const std::string& b,
                          const std::string& type = "i32");
    std::string transformLine(const std::string& line);
};

}
```

### MBAMult

```cpp
namespace morphect::mba {

class MBAMult {
public:
    std::string transform(const std::string& a, const std::string& b,
                          const std::string& type = "i32");

    // Constant multiplication (more efficient)
    std::string transformConstant(const std::string& a, int constant,
                                  const std::string& type = "i32");

    std::string transformLine(const std::string& line);
};

}
```

### LLVMMBAPass

```cpp
namespace morphect::mba {

class LLVMMBAPass : public LLVMTransformationPass {
public:
    LLVMMBAPass();

    std::string getName() const override;
    PassPriority getPriority() const override;

    TransformResult transformIR(std::vector<std::string>& lines) override;

    // Configuration
    void setChainDepth(int depth);
    void enableOperation(const std::string& op, bool enable);
};

}
```

---

## Control Flow API

### LLVMCFGAnalyzer

```cpp
#include "passes/cff/llvm_cfg_analyzer.hpp"

namespace morphect::cff {

struct BasicBlockInfo {
    int id;
    std::string label;
    std::vector<std::string> code;
    std::string terminator;
    std::vector<int> predecessors;
    std::vector<int> successors;

    bool is_entry = false;
    bool is_exit = false;
    bool has_conditional = false;
    bool has_switch = false;
    bool is_loop_header = false;
    bool is_landing_pad = false;

    std::string condition;
    int true_target = -1;
    int false_target = -1;
};

struct CFGInfo {
    std::string function_name;
    std::vector<BasicBlockInfo> blocks;
    int entry_block = 0;
    std::vector<int> exit_blocks;

    int num_blocks = 0;
    int num_edges = 0;
    int num_conditionals = 0;
    int num_loops = 0;

    bool has_exception_handling = false;
};

class LLVMCFGAnalyzer {
public:
    LLVMCFGAnalyzer();

    std::optional<CFGInfo> analyze(const std::vector<std::string>& function_ir);

    bool isSuitableForFlattening(const CFGInfo& cfg,
                                  const CFFConfig& config) const;
};

}
```

### LLVMCFFTransformation

```cpp
#include "passes/cff/llvm_cff_transform.hpp"

namespace morphect::cff {

struct CFFConfig {
    bool enabled = true;
    double probability = 0.8;
    int min_blocks = 3;
    int max_blocks = 100;
    bool shuffle_states = true;
    std::string state_var_name = "_cff_state";
};

struct CFFResult {
    bool success = false;
    std::string error;
    std::vector<std::string> transformed_code;
    int original_blocks = 0;
    int flattened_blocks = 0;
    int states_created = 0;
};

class LLVMCFFTransformation {
public:
    LLVMCFFTransformation();

    CFFResult flatten(const CFGInfo& cfg, const CFFConfig& config);
};

}
```

### OpaquePredicateLibrary

```cpp
#include "passes/cff/opaque_predicates.hpp"

namespace morphect::cff {

enum class PredicateType {
    AlwaysTrue,
    AlwaysFalse
};

struct OpaquePredicate {
    std::string name;
    PredicateType type;
    std::string description;
};

class OpaquePredicateLibrary {
public:
    OpaquePredicateLibrary();

    // Get predicates
    const OpaquePredicate& getAlwaysTrue() const;
    const OpaquePredicate& getAlwaysFalse() const;
    const OpaquePredicate* getByName(const std::string& name) const;
    std::vector<std::string> getAvailablePredicates() const;

    // Generate LLVM IR
    std::pair<std::vector<std::string>, std::string>
    generateAlwaysTrue(const std::string& var1, const std::string& var2);

    std::pair<std::vector<std::string>, std::string>
    generateAlwaysFalse(const std::string& var1, const std::string& var2);
};

}
```

---

## Data Obfuscation API

### StringEncoding

```cpp
#include "passes/data/string_encoding.hpp"

namespace morphect::data {

enum class EncodingMethod {
    XOR,
    RollingXOR,
    RC4
};

struct StringEncodingConfig {
    bool enabled = true;
    EncodingMethod method = EncodingMethod::XOR;
    uint8_t key = 0x42;
    int min_length = 3;
};

class StringEncoding {
public:
    StringEncoding();
    explicit StringEncoding(const StringEncodingConfig& config);

    // Encode/decode
    std::string encode(const std::string& str) const;
    std::string decode(const std::string& encoded) const;

    // IR transformation
    std::vector<std::string> transformIR(
        const std::vector<std::string>& lines);

    // Generate decoder function
    std::string generateDecoder() const;
};

}
```

### VariableSplitting

```cpp
#include "passes/data/variable_splitting.hpp"

namespace morphect::data {

enum class SplitMethod {
    Additive,
    XOR
};

struct VariableSplitConfig {
    bool enabled = true;
    double probability = 0.5;
    SplitMethod method = SplitMethod::Additive;
    int max_splits = 10;
};

class VariableSplitting {
public:
    VariableSplitting();
    explicit VariableSplitting(const VariableSplitConfig& config);

    std::vector<std::string> transformIR(
        const std::vector<std::string>& lines);
};

}
```

---

## Utility API

### GlobalRandom

```cpp
#include "common/random.hpp"

namespace morphect {

class GlobalRandom {
public:
    // Seeding
    static void seed(uint64_t s);
    static uint64_t getSeed();

    // Generation
    static int nextInt(int min, int max);
    static double nextDouble();
    static bool decide(double probability);

    // Utilities
    template<typename T>
    static void shuffle(std::vector<T>& vec);

    template<typename T>
    static const T& choice(const std::vector<T>& vec);
};

}
```

**Usage:**

```cpp
morphect::GlobalRandom::seed(12345);  // Reproducible

int val = morphect::GlobalRandom::nextInt(0, 100);
bool apply = morphect::GlobalRandom::decide(0.8);  // 80% chance

std::vector<int> items = {1, 2, 3, 4, 5};
morphect::GlobalRandom::shuffle(items);
```

### Logging

```cpp
#include "common/logging.hpp"

namespace morphect {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static LogLevel getLevel();

    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warning(const std::string& msg);
    static void error(const std::string& msg);

    static void log(LogLevel level, const std::string& msg);
};

}
```

### JsonParser

```cpp
#include "common/json_parser.hpp"

namespace morphect {

class JsonParser {
public:
    static std::optional<MorphectConfig> parse(const std::string& json);
    static std::optional<MorphectConfig> parseFile(const std::string& path);
};

}
```

---

*Previous: [Architecture](07-architecture.md) | Next: [Troubleshooting](09-troubleshooting.md)*
