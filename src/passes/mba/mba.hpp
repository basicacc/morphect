/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * mba.hpp - Main include file for MBA (Mixed Boolean Arithmetic) module
 *
 * Include this file to access all MBA transformations.
 *
 * Usage:
 *   #include "passes/mba/mba.hpp"
 *
 *   morphect::mba::LLVMMBAPass pass;
 *   pass.transformIR(lines);
 */

#ifndef MORPHECT_MBA_HPP
#define MORPHECT_MBA_HPP

// Base definitions
#include "mba_base.hpp"

// Individual transformations
#include "mba_add.hpp"
#include "mba_sub.hpp"
#include "mba_xor.hpp"
#include "mba_and.hpp"
#include "mba_or.hpp"
#include "mba_mult.hpp"

// Unified pass
#include "mba_pass.hpp"

#endif // MORPHECT_MBA_HPP
