# Configuration Reference

Complete reference for Morphect configuration options.

---

## Contents

1. [Configuration Methods](#configuration-methods)
2. [JSON Configuration Format](#json-configuration-format)
3. [Global Options](#global-options)
4. [MBA Options](#mba-options)
5. [Control Flow Options](#control-flow-options)
6. [Data Obfuscation Options](#data-obfuscation-options)
7. [Anti-Analysis Options](#anti-analysis-options)
8. [Example Configurations](#example-configurations)

---

## Configuration Methods

Morphect accepts configuration through multiple methods:

| Method | Tool | Precedence |
|--------|------|------------|
| Command-line flags | morphect-ir | Highest |
| Plugin arguments | GCC plugin | Highest |
| Configuration file | Both | Medium |
| Default values | Both | Lowest |

Command-line options override configuration file settings.

### Configuration File Loading

**morphect-ir:**
```bash
morphect-ir --config config.json input.ll output.ll
```

**GCC Plugin:**
```bash
gcc -fplugin=./morphect_plugin.so \
    -fplugin-arg-morphect_plugin-config=config.json \
    source.c -o program
```

---

## JSON Configuration Format

Configuration files use JSON format:

```json
{
  "global": {
    "probability": 0.85,
    "verbose": false
  },
  "mba": {
    "enabled": true
  }
}
```

### Comments

Standard JSON does not support comments. Use a separate documentation file or descriptive key names.

### Validation

Invalid JSON or unknown keys produce errors. Verify configuration with:

```bash
python3 -m json.tool config.json
```

---

## Global Options

Options affecting all transformations.

```json
{
  "global": {
    "probability": 0.85,
    "verbose": false,
    "seed": 0,
    "include_functions": [],
    "exclude_functions": []
  }
}
```

### Option Reference

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `probability` | float | 0.85 | Base transformation probability (0.0-1.0) |
| `verbose` | bool | false | Enable detailed logging |
| `seed` | int | 0 | Random seed (0 = non-deterministic) |
| `include_functions` | array | [] | Functions to include (empty = all) |
| `exclude_functions` | array | [] | Functions to exclude |

### Function Filtering

Include only specific functions:

```json
{
  "global": {
    "include_functions": ["encrypt", "decrypt", "process_secret"]
  }
}
```

Exclude performance-critical functions:

```json
{
  "global": {
    "exclude_functions": ["inner_loop", "hot_path_*"]
  }
}
```

Wildcards (`*`) match any suffix.

---

## MBA Options

Mixed Boolean Arithmetic configuration.

```json
{
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
  }
}
```

### Option Reference

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable MBA transformations |
| `probability` | float | 0.85 | Transformation probability |
| `chain_depth` | int | 2 | Nesting depth (1-5) |
| `operations.add` | bool | true | Transform addition |
| `operations.sub` | bool | true | Transform subtraction |
| `operations.xor` | bool | true | Transform XOR |
| `operations.and` | bool | true | Transform AND |
| `operations.or` | bool | true | Transform OR |
| `operations.mul` | bool | true | Transform multiplication |

### Chain Depth

Higher depth increases complexity exponentially:

| Depth | Typical Expansion |
|-------|-------------------|
| 1 | 3-5 instructions |
| 2 | 10-20 instructions |
| 3 | 30-60 instructions |
| 4 | 100+ instructions |
| 5 | 300+ instructions |

Recommended: 2-3 for balance of protection and performance.

---

## Control Flow Options

### Control Flow Flattening

```json
{
  "cff": {
    "enabled": true,
    "probability": 0.8,
    "min_blocks": 3,
    "max_blocks": 100,
    "shuffle_states": true,
    "state_var_name": "_cff_state",
    "flatten_loops": true
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable CFF |
| `probability` | float | 0.8 | Flattening probability |
| `min_blocks` | int | 3 | Minimum blocks to flatten |
| `max_blocks` | int | 100 | Maximum blocks to flatten |
| `shuffle_states` | bool | true | Randomize state values |
| `state_var_name` | string | "_cff_state" | State variable name |
| `flatten_loops` | bool | true | Flatten functions with loops |

### Bogus Control Flow

```json
{
  "bogus": {
    "enabled": true,
    "probability": 0.5,
    "min_insertions": 1,
    "max_insertions": 5,
    "generate_dead_code": true,
    "dead_code_lines": 3
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable bogus control flow |
| `probability` | float | 0.5 | Insertion probability |
| `min_insertions` | int | 1 | Minimum insertions per function |
| `max_insertions` | int | 5 | Maximum insertions per function |
| `generate_dead_code` | bool | true | Add code in fake branches |
| `dead_code_lines` | int | 3 | Lines in dead branches |

### Indirect Branches

```json
{
  "indirect_branches": {
    "enabled": true,
    "probability": 0.5,
    "use_jump_tables": true
  }
}
```

### Indirect Calls

```json
{
  "indirect_calls": {
    "enabled": true,
    "probability": 0.4,
    "use_dispatch_table": true
  }
}
```

---

## Data Obfuscation Options

### Variable Splitting

```json
{
  "varsplit": {
    "enabled": true,
    "probability": 0.5,
    "method": "additive",
    "max_splits": 10
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable variable splitting |
| `probability` | float | 0.5 | Splitting probability |
| `method` | string | "additive" | Split method: additive, xor |
| `max_splits` | int | 10 | Maximum splits per function |

### String Encoding

```json
{
  "strenc": {
    "enabled": true,
    "method": "xor",
    "key": 123,
    "min_length": 3,
    "exclude_patterns": ["%", "\\n"]
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable string encoding |
| `method` | string | "xor" | Encoding method |
| `key` | int | 123 | Encryption key |
| `min_length` | int | 3 | Minimum string length |
| `exclude_patterns` | array | [] | Patterns to skip |

Available methods: `xor`, `rolling_xor`, `rc4`

### Constant Obfuscation

```json
{
  "constant_obf": {
    "enabled": true,
    "probability": 0.7,
    "min_value": 10,
    "use_mba": true
  }
}
```

---

## Anti-Analysis Options

### Dead Code

```json
{
  "deadcode": {
    "enabled": true,
    "probability": 0.3,
    "complexity": "medium",
    "max_blocks": 5
  }
}
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable dead code insertion |
| `probability` | float | 0.3 | Insertion probability |
| `complexity` | string | "medium" | Complexity: low, medium, high |
| `max_blocks` | int | 5 | Maximum dead blocks per function |

### Anti-Debugging

```json
{
  "antidebug": {
    "enabled": false,
    "ptrace_check": true,
    "timing_check": true,
    "tracer_pid_check": true,
    "env_check": true
  }
}
```

### Anti-Disassembly

```json
{
  "antidisasm": {
    "enabled": false,
    "junk_bytes": true,
    "overlapping_instructions": true
  }
}
```

---

## Example Configurations

### Minimal Protection

Light obfuscation with minimal performance impact:

```json
{
  "global": {
    "probability": 0.5
  },
  "mba": {
    "enabled": true,
    "chain_depth": 1
  },
  "cff": {
    "enabled": false
  },
  "bogus": {
    "enabled": false
  }
}
```

### Standard Protection

Balanced protection and performance:

```json
{
  "global": {
    "probability": 0.85
  },
  "mba": {
    "enabled": true,
    "probability": 0.8,
    "chain_depth": 2
  },
  "cff": {
    "enabled": true,
    "probability": 0.7,
    "max_blocks": 50
  },
  "bogus": {
    "enabled": true,
    "probability": 0.5
  },
  "deadcode": {
    "enabled": true,
    "probability": 0.3
  }
}
```

### Maximum Protection

Highest obfuscation for sensitive code:

```json
{
  "global": {
    "probability": 1.0
  },
  "mba": {
    "enabled": true,
    "probability": 1.0,
    "chain_depth": 3
  },
  "cff": {
    "enabled": true,
    "probability": 1.0
  },
  "bogus": {
    "enabled": true,
    "probability": 0.8,
    "max_insertions": 10
  },
  "varsplit": {
    "enabled": true,
    "probability": 0.8
  },
  "deadcode": {
    "enabled": true,
    "probability": 0.5,
    "complexity": "high"
  }
}
```

### Selective Protection

Protect only specific functions:

```json
{
  "global": {
    "probability": 1.0,
    "include_functions": [
      "verify_license",
      "decrypt_key",
      "check_signature"
    ]
  },
  "mba": {
    "enabled": true,
    "chain_depth": 3
  },
  "cff": {
    "enabled": true
  }
}
```

---

*Previous: [Obfuscation Techniques](05-techniques.md) | Next: [Architecture](07-architecture.md)*
