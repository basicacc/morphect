/**
 * Morphect GCC GIMPLE Plugin
 *
 * A GCC plugin that applies Mixed Boolean Arithmetic (MBA) transformations
 * to obfuscate arithmetic operations at the GIMPLE level.
 *
 * Build: g++ -std=c++17 -shared -fPIC -fno-rtti \
 *        -I$(gcc -print-file-name=plugin)/include \
 *        gimple_obf_plugin.cpp -o morphect_plugin.so
 *
 * Usage: gcc -O0 -fplugin=./morphect_plugin.so source.c -o output
 */

// GCC plugin headers first
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "gimple.h"
#include "tree-pass.h"
#include "context.h"
#include "basic-block.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "stringpool.h"
// For make_ssa_name - need to include value-range.h before tree-ssanames.h on GCC 12+
#if __GNUC__ >= 12
#include "value-range.h"
#endif
#include "tree-ssanames.h"

#include <random>
#include <string>
#include <cstring>

// Plugin identification
int plugin_is_GPL_compatible;

static struct plugin_info morphect_plugin_info = {
    "1.0.1",  // version
    "Morphect obfuscator plugin.\n"
    "Options:\n"
    "  -fplugin-arg-morphect_plugin-probability=<n>  Probability (0.0-1.0)\n"
    "  -fplugin-arg-morphect_plugin-verbose          Enable verbose output\n"  // help
};

namespace {

// Simple RNG
std::mt19937_64 rng(std::random_device{}());
double global_probability = 0.85;
bool verbose = false;
int transforms_applied = 0;

bool decide(double prob) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < prob;
}

// Check if statement is an MBA candidate
bool is_mba_candidate(gimple* stmt) {
    if (!is_gimple_assign(stmt)) return false;

    enum tree_code code = gimple_assign_rhs_code(stmt);
    switch (code) {
        case PLUS_EXPR:
        case MINUS_EXPR:
        case BIT_XOR_EXPR:
        case BIT_AND_EXPR:
        case BIT_IOR_EXPR:
            return true;
        default:
            return false;
    }
}

// Transform: a + b = (a ^ b) + 2*(a & b)
bool transform_add(gimple_stmt_iterator* gsi, gimple* stmt) {
    tree lhs = gimple_assign_lhs(stmt);
    tree op1 = gimple_assign_rhs1(stmt);
    tree op2 = gimple_assign_rhs2(stmt);
    tree type = TREE_TYPE(lhs);

    if (!INTEGRAL_TYPE_P(type)) return false;
    if (!decide(global_probability)) return false;

    location_t loc = gimple_location(stmt);

    tree temp_xor = make_ssa_name(type);
    tree temp_and = make_ssa_name(type);
    tree temp_shl = make_ssa_name(type);

    gimple* g1 = gimple_build_assign(temp_xor, BIT_XOR_EXPR, op1, op2);
    gimple_set_location(g1, loc);

    gimple* g2 = gimple_build_assign(temp_and, BIT_AND_EXPR, op1, op2);
    gimple_set_location(g2, loc);

    tree one = build_int_cst(type, 1);
    gimple* g3 = gimple_build_assign(temp_shl, LSHIFT_EXPR, temp_and, one);
    gimple_set_location(g3, loc);

    gimple* g4 = gimple_build_assign(lhs, PLUS_EXPR, temp_xor, temp_shl);
    gimple_set_location(g4, loc);

    gsi_insert_before(gsi, g1, GSI_SAME_STMT);
    gsi_insert_before(gsi, g2, GSI_SAME_STMT);
    gsi_insert_before(gsi, g3, GSI_SAME_STMT);
    gsi_insert_before(gsi, g4, GSI_SAME_STMT);
    gsi_remove(gsi, true);

    transforms_applied++;
    return true;
}

// Transform: a ^ b = (a | b) - (a & b)
bool transform_xor(gimple_stmt_iterator* gsi, gimple* stmt) {
    tree lhs = gimple_assign_lhs(stmt);
    tree op1 = gimple_assign_rhs1(stmt);
    tree op2 = gimple_assign_rhs2(stmt);
    tree type = TREE_TYPE(lhs);

    if (!INTEGRAL_TYPE_P(type)) return false;
    if (!decide(global_probability)) return false;

    location_t loc = gimple_location(stmt);

    tree temp_or = make_ssa_name(type);
    tree temp_and = make_ssa_name(type);

    gimple* g1 = gimple_build_assign(temp_or, BIT_IOR_EXPR, op1, op2);
    gimple_set_location(g1, loc);

    gimple* g2 = gimple_build_assign(temp_and, BIT_AND_EXPR, op1, op2);
    gimple_set_location(g2, loc);

    gimple* g3 = gimple_build_assign(lhs, MINUS_EXPR, temp_or, temp_and);
    gimple_set_location(g3, loc);

    gsi_insert_before(gsi, g1, GSI_SAME_STMT);
    gsi_insert_before(gsi, g2, GSI_SAME_STMT);
    gsi_insert_before(gsi, g3, GSI_SAME_STMT);
    gsi_remove(gsi, true);

    transforms_applied++;
    return true;
}

// Transform: a & b = (a | b) - (a ^ b)
bool transform_and(gimple_stmt_iterator* gsi, gimple* stmt) {
    tree lhs = gimple_assign_lhs(stmt);
    tree op1 = gimple_assign_rhs1(stmt);
    tree op2 = gimple_assign_rhs2(stmt);
    tree type = TREE_TYPE(lhs);

    if (!INTEGRAL_TYPE_P(type)) return false;
    if (!decide(global_probability)) return false;

    location_t loc = gimple_location(stmt);

    tree temp_or = make_ssa_name(type);
    tree temp_xor = make_ssa_name(type);

    gimple* g1 = gimple_build_assign(temp_or, BIT_IOR_EXPR, op1, op2);
    gimple_set_location(g1, loc);

    gimple* g2 = gimple_build_assign(temp_xor, BIT_XOR_EXPR, op1, op2);
    gimple_set_location(g2, loc);

    gimple* g3 = gimple_build_assign(lhs, MINUS_EXPR, temp_or, temp_xor);
    gimple_set_location(g3, loc);

    gsi_insert_before(gsi, g1, GSI_SAME_STMT);
    gsi_insert_before(gsi, g2, GSI_SAME_STMT);
    gsi_insert_before(gsi, g3, GSI_SAME_STMT);
    gsi_remove(gsi, true);

    transforms_applied++;
    return true;
}

// Transform: a | b = (a ^ b) + (a & b)
bool transform_or(gimple_stmt_iterator* gsi, gimple* stmt) {
    tree lhs = gimple_assign_lhs(stmt);
    tree op1 = gimple_assign_rhs1(stmt);
    tree op2 = gimple_assign_rhs2(stmt);
    tree type = TREE_TYPE(lhs);

    if (!INTEGRAL_TYPE_P(type)) return false;
    if (!decide(global_probability)) return false;

    location_t loc = gimple_location(stmt);

    tree temp_xor = make_ssa_name(type);
    tree temp_and = make_ssa_name(type);

    gimple* g1 = gimple_build_assign(temp_xor, BIT_XOR_EXPR, op1, op2);
    gimple_set_location(g1, loc);

    gimple* g2 = gimple_build_assign(temp_and, BIT_AND_EXPR, op1, op2);
    gimple_set_location(g2, loc);

    gimple* g3 = gimple_build_assign(lhs, PLUS_EXPR, temp_xor, temp_and);
    gimple_set_location(g3, loc);

    gsi_insert_before(gsi, g1, GSI_SAME_STMT);
    gsi_insert_before(gsi, g2, GSI_SAME_STMT);
    gsi_insert_before(gsi, g3, GSI_SAME_STMT);
    gsi_remove(gsi, true);

    transforms_applied++;
    return true;
}

// Transform a statement
bool transform_statement(gimple_stmt_iterator* gsi) {
    gimple* stmt = gsi_stmt(*gsi);

    if (!is_mba_candidate(stmt)) return false;

    enum tree_code code = gimple_assign_rhs_code(stmt);
    switch (code) {
        case PLUS_EXPR:    return transform_add(gsi, stmt);
        case BIT_XOR_EXPR: return transform_xor(gsi, stmt);
        case BIT_AND_EXPR: return transform_and(gsi, stmt);
        case BIT_IOR_EXPR: return transform_or(gsi, stmt);
        default:           return false;
    }
}

// Execute pass on function
unsigned int execute_morphect_pass(function* fun) {
    if (verbose) {
        fprintf(stderr, "[morphect] Processing: %s\n", function_name(fun));
    }

    basic_block bb;
    FOR_EACH_BB_FN(bb, fun) {
        gimple_stmt_iterator gsi = gsi_start_bb(bb);
        while (!gsi_end_p(gsi)) {
            if (transform_statement(&gsi)) {
                if (gsi_end_p(gsi)) break;
            } else {
                gsi_next(&gsi);
            }
        }
    }

    return 0;
}

// Pass definition
const pass_data morphect_pass_data = {
    GIMPLE_PASS,
    "morphect",
    OPTGROUP_NONE,
    TV_NONE,
    PROP_ssa,
    0, 0, 0, 0
};

class morphect_gimple_pass : public gimple_opt_pass {
public:
    morphect_gimple_pass(gcc::context* ctx)
        : gimple_opt_pass(morphect_pass_data, ctx) {}

    bool gate(function*) override { return true; }

    unsigned int execute(function* fun) override {
        return execute_morphect_pass(fun);
    }
};

void print_stats(void*, void*) {
    if (verbose || transforms_applied > 0) {
        fprintf(stderr, "[morphect] Transformations applied: %d\n", transforms_applied);
    }
}

} // anonymous namespace

// Plugin init
int plugin_init(struct plugin_name_args* plugin_info,
                struct plugin_gcc_version* version) {

    if (!plugin_default_version_check(version, &gcc_version)) {
        fprintf(stderr, "morphect: GCC version mismatch\n");
        return 1;
    }

    register_callback(plugin_info->base_name, PLUGIN_INFO, nullptr, &morphect_plugin_info);

    // Parse args
    for (int i = 0; i < plugin_info->argc; i++) {
        const char* key = plugin_info->argv[i].key;
        const char* value = plugin_info->argv[i].value;

        if (strcmp(key, "probability") == 0 && value) {
            global_probability = atof(value);
        } else if (strcmp(key, "verbose") == 0) {
            verbose = true;
        }
    }

    if (verbose) {
        fprintf(stderr, "[morphect] Plugin loaded, probability: %.0f%%\n",
                global_probability * 100);
    }

    // Register pass
    struct register_pass_info pass_info;
    pass_info.pass = new morphect_gimple_pass(g);
    pass_info.reference_pass_name = "ssa";
    pass_info.ref_pass_instance_number = 1;
    pass_info.pos_op = PASS_POS_INSERT_AFTER;

    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, nullptr, &pass_info);
    register_callback(plugin_info->base_name, PLUGIN_FINISH, print_stats, nullptr);

    return 0;
}
