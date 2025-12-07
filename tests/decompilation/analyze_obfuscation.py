#!/usr/bin/env python3
"""
Morphect - Obfuscation Quality Analysis

This script analyzes compiled binaries to measure obfuscation effectiveness.
It compares normal and obfuscated versions to compute metrics like:
- Code size increase
- Instruction complexity
- Control flow complexity
- Entropy increase

Usage:
    python analyze_obfuscation.py normal_binary obfuscated_binary

Requirements:
    - objdump (for disassembly)
    - Python 3.6+
"""

import sys
import subprocess
import re
import math
from collections import Counter
from pathlib import Path


def run_objdump(binary_path):
    """Run objdump and return disassembly."""
    result = subprocess.run(
        ['objdump', '-d', binary_path],
        capture_output=True,
        text=True
    )
    return result.stdout


def parse_functions(disassembly):
    """Parse disassembly into functions."""
    functions = {}
    current_func = None
    current_instructions = []

    for line in disassembly.split('\n'):
        # Function header: 0000000000001234 <function_name>:
        func_match = re.match(r'^[0-9a-f]+ <([^>]+)>:', line)
        if func_match:
            if current_func:
                functions[current_func] = current_instructions
            current_func = func_match.group(1)
            current_instructions = []
            continue

        # Instruction line: address: bytes instruction
        instr_match = re.match(r'^\s+[0-9a-f]+:\s+([0-9a-f ]+)\s+(\S+)', line)
        if instr_match and current_func:
            bytes_hex = instr_match.group(1).strip()
            mnemonic = instr_match.group(2)
            current_instructions.append({
                'bytes': bytes_hex,
                'mnemonic': mnemonic,
                'full_line': line.strip()
            })

    if current_func:
        functions[current_func] = current_instructions

    return functions


def calculate_entropy(data):
    """Calculate Shannon entropy of data."""
    if not data:
        return 0

    counter = Counter(data)
    total = len(data)
    entropy = 0

    for count in counter.values():
        prob = count / total
        if prob > 0:
            entropy -= prob * math.log2(prob)

    return entropy


def analyze_function(instructions):
    """Analyze a function and return metrics."""
    if not instructions:
        return None

    metrics = {
        'instruction_count': len(instructions),
        'unique_mnemonics': len(set(i['mnemonic'] for i in instructions)),
        'byte_count': 0,
        'branch_count': 0,
        'call_count': 0,
        'arithmetic_count': 0,
        'bitwise_count': 0,
        'memory_count': 0,
    }

    branch_mnemonics = {'je', 'jne', 'jg', 'jge', 'jl', 'jle', 'ja', 'jae',
                        'jb', 'jbe', 'jmp', 'jo', 'jno', 'js', 'jns', 'jz', 'jnz'}
    arithmetic_mnemonics = {'add', 'sub', 'mul', 'imul', 'div', 'idiv', 'inc', 'dec', 'neg'}
    bitwise_mnemonics = {'and', 'or', 'xor', 'not', 'shl', 'shr', 'sar', 'sal', 'rol', 'ror'}
    memory_mnemonics = {'mov', 'movzx', 'movsx', 'lea', 'push', 'pop'}

    mnemonics = []
    for instr in instructions:
        mnemonic = instr['mnemonic'].lower()
        mnemonics.append(mnemonic)

        # Count bytes
        byte_str = instr['bytes'].replace(' ', '')
        metrics['byte_count'] += len(byte_str) // 2

        # Categorize instruction
        if mnemonic in branch_mnemonics:
            metrics['branch_count'] += 1
        elif mnemonic.startswith('call'):
            metrics['call_count'] += 1
        elif mnemonic in arithmetic_mnemonics:
            metrics['arithmetic_count'] += 1
        elif mnemonic in bitwise_mnemonics:
            metrics['bitwise_count'] += 1
        elif mnemonic in memory_mnemonics:
            metrics['memory_count'] += 1

    # Calculate entropy
    metrics['mnemonic_entropy'] = calculate_entropy(mnemonics)

    return metrics


def compare_binaries(normal_path, obfuscated_path):
    """Compare normal and obfuscated binaries."""
    print("=" * 60)
    print("Morphect Obfuscation Quality Analysis")
    print("=" * 60)
    print(f"\nNormal binary: {normal_path}")
    print(f"Obfuscated binary: {obfuscated_path}\n")

    # Get file sizes
    normal_size = Path(normal_path).stat().st_size
    obfuscated_size = Path(obfuscated_path).stat().st_size

    print(f"Binary sizes:")
    print(f"  Normal: {normal_size} bytes")
    print(f"  Obfuscated: {obfuscated_size} bytes")
    print(f"  Increase: {obfuscated_size - normal_size} bytes " +
          f"({(obfuscated_size - normal_size) * 100 / normal_size:.1f}%)")

    # Parse disassembly
    normal_disasm = run_objdump(normal_path)
    obfuscated_disasm = run_objdump(obfuscated_path)

    normal_funcs = parse_functions(normal_disasm)
    obfuscated_funcs = parse_functions(obfuscated_disasm)

    # Find common functions (exclude system functions)
    interesting_funcs = [
        'simple_hash', 'check_license', 'process_command',
        'fibonacci', 'state_machine', 'encrypt_round', 'xor_cipher', 'main'
    ]

    print("\n" + "=" * 60)
    print("Per-Function Analysis")
    print("=" * 60)

    total_normal = {'instructions': 0, 'bytes': 0, 'branches': 0}
    total_obfuscated = {'instructions': 0, 'bytes': 0, 'branches': 0}

    for func_name in interesting_funcs:
        normal_match = None
        obfuscated_match = None

        # Find matching function names (may have prefixes)
        for name in normal_funcs:
            if func_name in name:
                normal_match = name
                break

        for name in obfuscated_funcs:
            if func_name in name:
                obfuscated_match = name
                break

        if not normal_match or not obfuscated_match:
            continue

        normal_metrics = analyze_function(normal_funcs[normal_match])
        obfuscated_metrics = analyze_function(obfuscated_funcs[obfuscated_match])

        if not normal_metrics or not obfuscated_metrics:
            continue

        print(f"\n--- {func_name} ---")
        print(f"  Instructions: {normal_metrics['instruction_count']} -> " +
              f"{obfuscated_metrics['instruction_count']} " +
              f"({obfuscated_metrics['instruction_count'] - normal_metrics['instruction_count']:+d})")
        print(f"  Bytes: {normal_metrics['byte_count']} -> " +
              f"{obfuscated_metrics['byte_count']} " +
              f"({obfuscated_metrics['byte_count'] - normal_metrics['byte_count']:+d})")
        print(f"  Branches: {normal_metrics['branch_count']} -> " +
              f"{obfuscated_metrics['branch_count']} " +
              f"({obfuscated_metrics['branch_count'] - normal_metrics['branch_count']:+d})")
        print(f"  Unique mnemonics: {normal_metrics['unique_mnemonics']} -> " +
              f"{obfuscated_metrics['unique_mnemonics']}")
        print(f"  Mnemonic entropy: {normal_metrics['mnemonic_entropy']:.2f} -> " +
              f"{obfuscated_metrics['mnemonic_entropy']:.2f}")

        # Accumulate totals
        total_normal['instructions'] += normal_metrics['instruction_count']
        total_normal['bytes'] += normal_metrics['byte_count']
        total_normal['branches'] += normal_metrics['branch_count']

        total_obfuscated['instructions'] += obfuscated_metrics['instruction_count']
        total_obfuscated['bytes'] += obfuscated_metrics['byte_count']
        total_obfuscated['branches'] += obfuscated_metrics['branch_count']

    # Summary
    print("\n" + "=" * 60)
    print("Summary (Analyzed Functions)")
    print("=" * 60)

    if total_normal['instructions'] > 0:
        instr_increase = (total_obfuscated['instructions'] - total_normal['instructions']) * 100 / total_normal['instructions']
        byte_increase = (total_obfuscated['bytes'] - total_normal['bytes']) * 100 / total_normal['bytes']
        branch_increase = (total_obfuscated['branches'] - total_normal['branches']) * 100 / total_normal['branches'] if total_normal['branches'] > 0 else 0

        print(f"\nTotal instruction count: {total_normal['instructions']} -> {total_obfuscated['instructions']} ({instr_increase:+.1f}%)")
        print(f"Total byte count: {total_normal['bytes']} -> {total_obfuscated['bytes']} ({byte_increase:+.1f}%)")
        print(f"Total branch count: {total_normal['branches']} -> {total_obfuscated['branches']} ({branch_increase:+.1f}%)")

        # Quality score (rough estimate)
        quality_score = min(100, (instr_increase + byte_increase + branch_increase) / 3)
        print(f"\nObfuscation quality score: {quality_score:.0f}/100")

        if quality_score < 10:
            print("Rating: MINIMAL - Obfuscation had little effect")
        elif quality_score < 30:
            print("Rating: LOW - Some obfuscation applied")
        elif quality_score < 60:
            print("Rating: MODERATE - Good obfuscation coverage")
        elif quality_score < 100:
            print("Rating: HIGH - Strong obfuscation")
        else:
            print("Rating: EXTREME - Very aggressive obfuscation")

    print("")


def main():
    if len(sys.argv) != 3:
        print("Usage: python analyze_obfuscation.py <normal_binary> <obfuscated_binary>")
        sys.exit(1)

    normal_path = sys.argv[1]
    obfuscated_path = sys.argv[2]

    if not Path(normal_path).exists():
        print(f"Error: Normal binary not found: {normal_path}")
        sys.exit(1)

    if not Path(obfuscated_path).exists():
        print(f"Error: Obfuscated binary not found: {obfuscated_path}")
        sys.exit(1)

    compare_binaries(normal_path, obfuscated_path)


if __name__ == '__main__':
    main()
