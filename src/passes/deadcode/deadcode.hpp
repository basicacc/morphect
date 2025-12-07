/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * deadcode.hpp - Main include file for dead code obfuscation module
 *
 * Include this file to access all dead code generation features.
 *
 * Usage:
 *   #include "passes/deadcode/deadcode.hpp"
 *
 *   morphect::deadcode::LLVMDeadCodePass pass;
 *   pass.transformIR(lines);
 */

#ifndef MORPHECT_DEADCODE_HPP
#define MORPHECT_DEADCODE_HPP

// Base definitions
#include "dead_code_base.hpp"

// LLVM IR implementation
#include "dead_code.hpp"

#endif // MORPHECT_DEADCODE_HPP
