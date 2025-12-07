/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * data.hpp - Main include file for data obfuscation module
 *
 * Include this file to access all data obfuscation passes.
 *
 * Usage:
 *   #include "passes/data/data.hpp"
 *
 *   morphect::data::LLVMStringEncodingPass str_pass;
 *   morphect::data::LLVMConstantObfPass const_pass;
 *   morphect::data::LLVMVariableSplittingPass split_pass;
 */

#ifndef MORPHECT_DATA_HPP
#define MORPHECT_DATA_HPP

// Base definitions
#include "data_base.hpp"

// Individual passes
#include "string_encoding.hpp"
#include "constant_obf.hpp"
#include "variable_splitting.hpp"

#endif // MORPHECT_DATA_HPP
