/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * control_flow.hpp - Main include file for control flow obfuscation module
 *
 * Include this file to access all control flow obfuscation passes.
 *
 * Usage:
 *   #include "passes/control_flow/control_flow.hpp"
 *
 *   morphect::control_flow::LLVMIndirectBranchPass ib_pass;
 *   morphect::control_flow::LLVMIndirectCallPass ic_pass;
 *   morphect::control_flow::LLVMCallStackObfPass cso_pass;
 */

#ifndef MORPHECT_CONTROL_FLOW_HPP
#define MORPHECT_CONTROL_FLOW_HPP

// Base definitions
#include "indirect_branch_base.hpp"
#include "indirect_call_base.hpp"
#include "call_stack_obf_base.hpp"

// Individual passes
#include "indirect_branch.hpp"
#include "indirect_call.hpp"
#include "call_stack_obf.hpp"

#endif // MORPHECT_CONTROL_FLOW_HPP
