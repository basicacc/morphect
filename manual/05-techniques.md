# Obfuscation Techniques

Detailed explanation of the obfuscation methods implemented in Morphect.

---

## Contents

1. [Mixed Boolean Arithmetic](#mixed-boolean-arithmetic)
2. [Control Flow Flattening](#control-flow-flattening)
3. [Bogus Control Flow](#bogus-control-flow)
4. [Opaque Predicates](#opaque-predicates)
5. [Variable Splitting](#variable-splitting)
6. [String Encoding](#string-encoding)
7. [Dead Code Insertion](#dead-code-insertion)
8. [Indirect Control Flow](#indirect-control-flow)
9. [Anti-Debugging](#anti-debugging)
10. [Anti-Disassembly](#anti-disassembly)

---

## Mixed Boolean Arithmetic

Mixed Boolean Arithmetic (MBA) transforms simple arithmetic and bitwise operations into complex but mathematically equivalent expressions. The transformations are based on Boolean algebra identities.

### Mathematical Basis

For any integers `a` and `b`:

```
a + b = (a ^ b) + 2 * (a & b)
a - b = (a ^ ~b) + 2 * (a & ~b) + 1
a ^ b = (a | b) - (a & b)
a & b = (a | b) - (a ^ b)
a | b = (a ^ b) + (a & b)
```

These identities hold for all bit widths and signedness.

### Implementation

Consider the addition `x + y`:

**Original IR:**
```llvm
%sum = add i32 %x, %y
```

**After MBA transformation:**
```llvm
%t1 = xor i32 %x, %y           ; a ^ b
%t2 = and i32 %x, %y           ; a & b
%t3 = shl i32 %t2, 1           ; 2 * (a & b)
%sum = add i32 %t1, %t3        ; (a ^ b) + 2 * (a & b)
```

### Chaining

Transformations can be nested for increased complexity:

**Level 1:**
```
a + b = (a ^ b) + 2 * (a & b)
```

**Level 2:** Apply MBA to the XOR:
```
a ^ b = (a | b) - (a & b)
```

**Result:**
```
a + b = ((a | b) - (a & b)) + 2 * (a & b)
      = (a | b) + (a & b)
```

Each nesting level increases code size and analysis difficulty.

### Multiplication

Multiplication by constants is transformed using decomposition:

```
a * 5 = a * 4 + a * 1
      = (a << 2) + a
```

The individual additions then undergo MBA transformation.

### Effectiveness

MBA increases the number of instructions and data dependencies, making it difficult for:

- Pattern matching in decompilers
- Symbolic execution engines
- Manual analysis

Performance impact is low (typically 2-5%) because modern CPUs execute bitwise operations efficiently.

---

## Control Flow Flattening

Control Flow Flattening (CFF) transforms structured code into a state machine, obscuring the original program logic.

### Concept

Original control flow:

```
        Entry
          |
       [cond?]
       /    \
     True  False
       \    /
        Exit
```

Flattened control flow:

```
        Entry
          |
    +-> Dispatcher
    |      |
    |   [switch]
    |   /  |  \
    | S0  S1  S2
    |   \  |  /
    +-----+
```

All basic blocks become cases in a central switch statement. A state variable determines which block executes next.

### Transformation Steps

1. **Analysis:** Identify all basic blocks and their successors
2. **State Assignment:** Assign a unique state number to each block
3. **Dispatcher Generation:** Create the switch-based dispatcher
4. **Block Conversion:** Convert each block to a switch case
5. **Terminator Replacement:** Replace branches with state updates

### Example

**Original C code:**
```c
int classify(int x) {
    if (x < 0)
        return -1;
    else if (x == 0)
        return 0;
    else
        return 1;
}
```

**Flattened equivalent:**
```c
int classify_flat(int x) {
    int state = 0;
    int result;

    while (1) {
        switch (state) {
            case 0:  // Entry
                if (x < 0)
                    state = 1;
                else
                    state = 2;
                break;
            case 1:  // x < 0
                result = -1;
                state = 5;  // Exit
                break;
            case 2:  // x >= 0
                if (x == 0)
                    state = 3;
                else
                    state = 4;
                break;
            case 3:  // x == 0
                result = 0;
                state = 5;
                break;
            case 4:  // x > 0
                result = 1;
                state = 5;
                break;
            case 5:  // Exit
                return result;
        }
    }
}
```

### PHI Node Handling

SSA PHI nodes require special treatment because all paths now flow through the dispatcher:

**Original:**
```llvm
%result = phi i32 [ %a, %block1 ], [ %b, %block2 ]
```

**Flattened:** PHI values are stored to memory before state transitions and loaded when needed.

### State Obfuscation

State values can be randomized to prevent pattern recognition:

- Sequential: `0, 1, 2, 3, ...`
- Randomized: `847, 2391, 156, 4782, ...`

### Effectiveness

CFF dramatically changes the control flow graph structure. Decompilers produce confusing output with nested loops and switches. Manual analysis requires reconstructing the original state machine.

Performance impact is moderate (5-15%) due to the dispatcher overhead.

---

## Bogus Control Flow

Bogus Control Flow inserts fake execution paths that are never taken, cluttering the control flow graph.

### Implementation

```llvm
; Original
%result = add i32 %a, %b

; With bogus control flow
%pred = call i1 @opaque_true()
br i1 %pred, label %real, label %fake

real:
  %result = add i32 %a, %b
  br label %continue

fake:                           ; Never executed
  %bogus = sub i32 %a, %b      ; Looks legitimate
  br label %continue

continue:
  ; ...
```

### Effect on Analysis

Static analysis tools must consider both paths, increasing complexity. The fake path contains realistic-looking code that wastes analyst time.

---

## Opaque Predicates

Opaque predicates are conditions whose values are known at obfuscation time but difficult to determine through static analysis.

### Categories

**Always True:**
- `x * x >= 0` (squares are non-negative)
- `(x | y) >= (x & y)` (OR produces bits, AND only keeps them)
- `(x ^ x) == 0` (XOR with self is zero)

**Always False:**
- `x * x < 0`
- `(x ^ x) != 0`

**Context-Dependent:**
- Based on loop invariants
- Based on type constraints

### Examples

**Even Product Predicate:**
```llvm
; x * (x + 1) is always even
%t1 = add i32 %x, 1
%t2 = mul i32 %x, %t1
%t3 = and i32 %t2, 1
%pred = icmp eq i32 %t3, 0    ; Always true
```

**MBA-Based Predicate:**
```llvm
; 2*(a&b) + (a^b) == a+b is always true
%t1 = and i32 %a, %b
%t2 = shl i32 %t1, 1
%t3 = xor i32 %a, %b
%t4 = add i32 %t2, %t3
%t5 = add i32 %a, %b
%pred = icmp eq i32 %t4, %t5  ; Always true
```

### Resilience

Good opaque predicates resist:
- Constant folding
- Interval analysis
- Symbolic execution (within timeout)

---

## Variable Splitting

Variable splitting divides a single variable into multiple components that are combined when the value is needed.

### Methods

**Additive:** `x = a + b`
```llvm
; Original: store 100
%part1 = 37
%part2 = 63
; When needed: %x = add %part1, %part2
```

**XOR:** `x = a ^ b`
```llvm
; Original: store 100
%part1 = 0xDEAD
%part2 = 0xDEAD ^ 100
; When needed: %x = xor %part1, %part2
```

**Multiplicative:** `x = a * b` (for factorizable values)

### Effect

Analysts must track multiple variables and understand their relationship. Simple data flow analysis becomes insufficient.

---

## String Encoding

String literals are encoded at compile time and decoded at runtime.

### Methods

**XOR Encoding:**
```c
// Original
"Hello" -> {0x48, 0x65, 0x6c, 0x6c, 0x6f}

// Encoded (key=0x42)
{0x0A, 0x27, 0x2E, 0x2E, 0x2D}
```

**Rolling XOR:**
Each byte is XORed with the previous decoded byte, creating a dependency chain.

**RC4:**
Full stream cipher encryption with configurable key.

### Runtime Decoding

Encoded strings require a decoder function:

```c
char* decode_xor(char* encoded, int len, char key) {
    for (int i = 0; i < len; i++)
        encoded[i] ^= key;
    return encoded;
}
```

---

## Dead Code Insertion

Dead code adds computations that do not affect program output.

### Techniques

**Arithmetic Dead Code:**
```llvm
%dead1 = mul i32 %x, 7
%dead2 = add i32 %dead1, 42
%dead3 = xor i32 %dead2, %dead1
; Results never used
```

**Control Flow Dead Code:**
```llvm
br i1 false, label %dead_block, label %real_block

dead_block:
  ; Complex code that never executes
  br label %real_block

real_block:
  ; Actual code
```

### Effectiveness

Dead code increases binary size and complicates analysis. Optimizing compilers can remove obvious dead code, so the inserted code must appear potentially live.

---

## Indirect Control Flow

### Indirect Branches

Direct branches are replaced with computed jumps:

```asm
; Original
jmp target

; Indirect
lea rax, [rip + target]
jmp rax
```

### Indirect Calls

Function calls go through pointers:

```c
// Original
result = function(args);

// Indirect
typedef int (*func_t)(int);
func_t ptr = &function;
result = ptr(args);
```

### Jump Tables

Multiple targets use computed indices:

```c
void* targets[] = {&&label1, &&label2, &&label3};
goto *targets[computed_index];
```

---

## Anti-Debugging

Techniques to detect or impede debuggers.

### ptrace Detection (Linux)

```c
if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
    // Being debugged
    exit(1);
}
```

### Timing Checks

```c
uint64_t start = rdtsc();
// ... some code ...
uint64_t end = rdtsc();
if (end - start > THRESHOLD) {
    // Likely being debugged (breakpoints add delay)
}
```

### TracerPid Check

```c
FILE* f = fopen("/proc/self/status", "r");
// Parse for TracerPid: if non-zero, being traced
```

---

## Anti-Disassembly

Techniques to confuse static disassemblers.

### Junk Bytes

Insert invalid bytes after unconditional jumps:

```asm
jmp real_target
.byte 0xE8          ; Looks like call instruction
real_target:
```

Disassemblers may interpret the junk byte as code, misaligning subsequent instructions.

### Overlapping Instructions

x86 instructions can overlap:

```asm
addr:     eb 01       jmp addr+3
addr+2:   e8 ...      (appears to be call)
addr+3:   ...         (actual next instruction)
```

The `call` instruction is never executed but confuses linear disassembly.

### Opaque Jumps

Conditional jumps with opaque predicates:

```asm
xor eax, eax
test eax, eax
jnz fake_target     ; Never taken
; real code
```

Disassemblers may analyze the fake path.

---

## Performance Summary

| Technique | Code Size | Runtime | Analysis Difficulty |
|-----------|-----------|---------|---------------------|
| MBA | +10-30% | +2-5% | High |
| CFF | +20-50% | +5-15% | Very High |
| Bogus CF | +20-40% | +2-5% | High |
| Variable Split | +5-15% | +1-3% | Medium |
| String Encoding | +5-10% | +1-2% | Medium |
| Dead Code | +10-30% | +1-3% | Low-Medium |
| Indirect CF | +5-15% | +2-5% | Medium |

---

*Previous: [GCC Plugin](04-gcc-plugin.md) | Next: [Configuration Reference](06-configuration.md)*
