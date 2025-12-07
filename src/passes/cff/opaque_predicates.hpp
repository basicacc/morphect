/*
 * opaque_predicates.hpp
 *
 * conditions that always eval to true/false but look complex
 * e.g. x*x >= 0 is always true but a decompiler won't see that
 */

#ifndef MORPHECT_OPAQUE_PREDICATES_HPP
#define MORPHECT_OPAQUE_PREDICATES_HPP

#include "../../common/random.hpp"
#include <string>
#include <vector>
#include <functional>
#include <climits>

namespace morphect {
namespace cff {

enum class PredicateType {
    AlwaysTrue,
    AlwaysFalse,
    ContextDependent  // uses existing vars
};

struct ContextPredicateInfo {
    std::string variable;
    std::string variable_type;      // i32, i64, etc
    bool is_loop_counter = false;
    bool is_array_size = false;
    int known_lower_bound = 0;
    int known_upper_bound = INT_MAX;
};

struct OpaquePredicate {
    std::string name;
    PredicateType type;
    std::string description;
    std::function<std::vector<std::string>(const std::string&, const std::string&)> llvm_generator;
};

struct ContextOpaquePredicate {
    std::string name;
    std::string description;
    bool requires_loop_counter = false;
    bool requires_array_size = false;
    bool requires_two_variables = false;

    std::function<std::pair<std::vector<std::string>, std::string>(
        const ContextPredicateInfo&, int&)> generator;

    std::function<std::pair<std::vector<std::string>, std::string>(
        const ContextPredicateInfo&,
        const ContextPredicateInfo&, int&)> dual_generator;
};

class OpaquePredicateLibrary {
public:
    OpaquePredicateLibrary();

    const OpaquePredicate& getAlwaysTrue();
    const OpaquePredicate& getAlwaysFalse();
    const OpaquePredicate* getByName(const std::string& name);
    const std::vector<OpaquePredicate>& getAllPredicates() const { return predicates_; }

    // returns (ir code, result var)
    std::pair<std::vector<std::string>, std::string>
    generateAlwaysTrue(const std::string& var1, const std::string& var2);

    std::pair<std::vector<std::string>, std::string>
    generateAlwaysFalse(const std::string& var1, const std::string& var2);

    const std::vector<ContextOpaquePredicate>& getContextPredicates() const {
        return context_predicates_;
    }

    const ContextOpaquePredicate* getContextPredicate(
        bool have_loop_counter, bool have_array_size, bool have_two_vars);

    std::pair<std::vector<std::string>, std::string>
    generateContextPredicate(const ContextPredicateInfo& ctx);

    std::pair<std::vector<std::string>, std::string>
    generateContextPredicate(const ContextPredicateInfo& ctx1,
                             const ContextPredicateInfo& ctx2);

private:
    std::vector<OpaquePredicate> predicates_;
    std::vector<size_t> true_indices_;
    std::vector<size_t> false_indices_;

    std::vector<ContextOpaquePredicate> context_predicates_;

    int temp_counter_ = 0;
    std::string nextTemp() {
        return "%_op_tmp" + std::to_string(temp_counter_++);
    }

    void initializePredicates();
    void initializeContextPredicates();
};

namespace predicates {

// (x * (x + 1)) % 2 == 0 -- consecutive ints multiply to even
inline std::vector<std::string> evenProduct(
    const std::string& x,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);
    std::string t3 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = add i32 " + x + ", 1");
    code.push_back("  " + t2 + " = mul i32 " + x + ", " + t1);
    code.push_back("  " + t3 + " = and i32 " + t2 + ", 1");
    code.push_back("  " + result_var + " = icmp eq i32 " + t3 + ", 0");

    return code;
}

// x * x >= 0 always
inline std::vector<std::string> squareNonNegative(
    const std::string& x,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = mul i32 " + x + ", " + x);
    code.push_back("  " + result_var + " = icmp sge i32 " + t1 + ", 0");

    return code;
}

// (x | y) >= (x & y) always
inline std::vector<std::string> orGeqAnd(
    const std::string& x,
    const std::string& y,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = or i32 " + x + ", " + y);
    code.push_back("  " + t2 + " = and i32 " + x + ", " + y);
    code.push_back("  " + result_var + " = icmp sge i32 " + t1 + ", " + t2);

    return code;
}

// x ^ x == 0 always
inline std::vector<std::string> xorSelfZero(
    const std::string& x,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = xor i32 " + x + ", " + x);
    code.push_back("  " + result_var + " = icmp eq i32 " + t1 + ", 0");

    return code;
}

// ((x & y) | (x ^ y)) == (x | y) -- boolean identity
inline std::vector<std::string> booleanIdentity(
    const std::string& x,
    const std::string& y,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);
    std::string t3 = "%_op_t" + std::to_string(temp_counter++);
    std::string t4 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = and i32 " + x + ", " + y);
    code.push_back("  " + t2 + " = xor i32 " + x + ", " + y);
    code.push_back("  " + t3 + " = or i32 " + t1 + ", " + t2);
    code.push_back("  " + t4 + " = or i32 " + x + ", " + y);
    code.push_back("  " + result_var + " = icmp eq i32 " + t3 + ", " + t4);

    return code;
}

// 2*(x&y) + (x^y) == x+y -- the MBA identity
inline std::vector<std::string> mbaIdentity(
    const std::string& x,
    const std::string& y,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);
    std::string t3 = "%_op_t" + std::to_string(temp_counter++);
    std::string t4 = "%_op_t" + std::to_string(temp_counter++);
    std::string t5 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = and i32 " + x + ", " + y);
    code.push_back("  " + t2 + " = shl i32 " + t1 + ", 1");
    code.push_back("  " + t3 + " = xor i32 " + x + ", " + y);
    code.push_back("  " + t4 + " = add i32 " + t2 + ", " + t3);
    code.push_back("  " + t5 + " = add i32 " + x + ", " + y);
    code.push_back("  " + result_var + " = icmp eq i32 " + t4 + ", " + t5);

    return code;
}

// always-false variants

// x*x < 0 never
inline std::vector<std::string> squareNegative(
    const std::string& x,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = mul i32 " + x + ", " + x);
    code.push_back("  " + result_var + " = icmp slt i32 " + t1 + ", 0");

    return code;
}

// x^x != 0 never
inline std::vector<std::string> xorSelfNonZero(
    const std::string& x,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = xor i32 " + x + ", " + x);
    code.push_back("  " + result_var + " = icmp ne i32 " + t1 + ", 0");

    return code;
}

// (x|y) < (x&y) never
inline std::vector<std::string> orLtAnd(
    const std::string& x,
    const std::string& y,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = or i32 " + x + ", " + y);
    code.push_back("  " + t2 + " = and i32 " + x + ", " + y);
    code.push_back("  " + result_var + " = icmp slt i32 " + t1 + ", " + t2);

    return code;
}

// context predicates - use existing program vars
using cff::ContextPredicateInfo;

// loop counter predicate: i*(i+1) is always even
inline std::vector<std::string> loopCounterEven(
    const ContextPredicateInfo& ctx,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);
    std::string t3 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = add " + ctx.variable_type + " " + ctx.variable + ", 1");
    code.push_back("  " + t2 + " = mul " + ctx.variable_type + " " + ctx.variable + ", " + t1);
    code.push_back("  " + t3 + " = and " + ctx.variable_type + " " + t2 + ", 1");
    code.push_back("  " + result_var + " = icmp eq " + ctx.variable_type + " " + t3 + ", 0");

    return code;
}

// array sizes are >= 0
inline std::vector<std::string> arraySizeNonNegative(
    const ContextPredicateInfo& ctx,
    const std::string& result_var,
    int& temp_counter) {

    (void)temp_counter;
    std::vector<std::string> code;
    code.push_back("  " + result_var + " = icmp sge " + ctx.variable_type + " " + ctx.variable + ", 0");

    return code;
}

// x & ~x == 0 always
inline std::vector<std::string> andNotSelf(
    const ContextPredicateInfo& ctx,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = xor " + ctx.variable_type + " " + ctx.variable + ", -1");
    code.push_back("  " + t2 + " = and " + ctx.variable_type + " " + ctx.variable + ", " + t1);
    code.push_back("  " + result_var + " = icmp eq " + ctx.variable_type + " " + t2 + ", 0");

    return code;
}

// if i < n then i - n < 0
inline std::vector<std::string> loopBoundCheck(
    const ContextPredicateInfo& counter_ctx,
    const ContextPredicateInfo& bound_ctx,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);
    std::string t3 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = icmp slt " + counter_ctx.variable_type + " " +
                   counter_ctx.variable + ", " + bound_ctx.variable);
    code.push_back("  " + t2 + " = sub " + counter_ctx.variable_type + " " +
                   counter_ctx.variable + ", " + bound_ctx.variable);
    code.push_back("  " + t3 + " = icmp slt " + counter_ctx.variable_type + " " + t2 + ", 0");
    code.push_back("  " + result_var + " = icmp eq i1 " + t1 + ", " + t3);

    return code;
}

// (x / y) * y + (x % y) == x always when y != 0
inline std::vector<std::string> divModIdentity(
    const ContextPredicateInfo& dividend_ctx,
    const std::string& divisor,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);
    std::string t3 = "%_op_t" + std::to_string(temp_counter++);
    std::string t4 = "%_op_t" + std::to_string(temp_counter++);

    code.push_back("  " + t1 + " = sdiv " + dividend_ctx.variable_type + " " +
                   dividend_ctx.variable + ", " + divisor);
    code.push_back("  " + t2 + " = mul " + dividend_ctx.variable_type + " " + t1 + ", " + divisor);
    code.push_back("  " + t3 + " = srem " + dividend_ctx.variable_type + " " +
                   dividend_ctx.variable + ", " + divisor);
    code.push_back("  " + t4 + " = add " + dividend_ctx.variable_type + " " + t2 + ", " + t3);
    code.push_back("  " + result_var + " = icmp eq " + dividend_ctx.variable_type + " " +
                   t4 + ", " + dividend_ctx.variable);

    return code;
}

// shift identity
inline std::vector<std::string> shiftMask(
    const ContextPredicateInfo& ctx,
    int shift_amount,
    int bit_width,
    const std::string& result_var,
    int& temp_counter) {

    std::vector<std::string> code;
    std::string t1 = "%_op_t" + std::to_string(temp_counter++);
    std::string t2 = "%_op_t" + std::to_string(temp_counter++);

    int mask = (1 << (bit_width - shift_amount)) - 1;

    code.push_back("  " + t1 + " = and " + ctx.variable_type + " " + ctx.variable +
                   ", " + std::to_string(mask));
    std::string t3 = "%_op_t" + std::to_string(temp_counter++);
    std::string t4 = "%_op_t" + std::to_string(temp_counter++);
    code.push_back("  " + t3 + " = shl " + ctx.variable_type + " " + ctx.variable +
                   ", " + std::to_string(shift_amount));
    code.push_back("  " + t4 + " = lshr " + ctx.variable_type + " " + t3 +
                   ", " + std::to_string(shift_amount));
    code.push_back("  " + result_var + " = icmp eq " + ctx.variable_type + " " + t1 + ", " + t4);

    return code;
}

} // namespace predicates

inline OpaquePredicateLibrary::OpaquePredicateLibrary() {
    initializePredicates();
    initializeContextPredicates();
}

inline void OpaquePredicateLibrary::initializePredicates() {
    predicates_.push_back({
        "even_product",
        PredicateType::AlwaysTrue,
        "(x * (x + 1)) % 2 == 0",
        [this](const std::string& x, const std::string&) {
            std::string result = nextTemp();
            return predicates::evenProduct(x, result, temp_counter_);
        }
    });
    true_indices_.push_back(predicates_.size() - 1);

    predicates_.push_back({
        "square_nonneg",
        PredicateType::AlwaysTrue,
        "x * x >= 0",
        [this](const std::string& x, const std::string&) {
            std::string result = nextTemp();
            return predicates::squareNonNegative(x, result, temp_counter_);
        }
    });
    true_indices_.push_back(predicates_.size() - 1);

    predicates_.push_back({
        "or_geq_and",
        PredicateType::AlwaysTrue,
        "(x | y) >= (x & y)",
        [this](const std::string& x, const std::string& y) {
            std::string result = nextTemp();
            return predicates::orGeqAnd(x, y, result, temp_counter_);
        }
    });
    true_indices_.push_back(predicates_.size() - 1);

    predicates_.push_back({
        "xor_self_zero",
        PredicateType::AlwaysTrue,
        "(x ^ x) == 0",
        [this](const std::string& x, const std::string&) {
            std::string result = nextTemp();
            return predicates::xorSelfZero(x, result, temp_counter_);
        }
    });
    true_indices_.push_back(predicates_.size() - 1);

    predicates_.push_back({
        "boolean_identity",
        PredicateType::AlwaysTrue,
        "((x & y) | (x ^ y)) == (x | y)",
        [this](const std::string& x, const std::string& y) {
            std::string result = nextTemp();
            return predicates::booleanIdentity(x, y, result, temp_counter_);
        }
    });
    true_indices_.push_back(predicates_.size() - 1);

    predicates_.push_back({
        "mba_identity",
        PredicateType::AlwaysTrue,
        "2 * (x & y) + (x ^ y) == x + y",
        [this](const std::string& x, const std::string& y) {
            std::string result = nextTemp();
            return predicates::mbaIdentity(x, y, result, temp_counter_);
        }
    });
    true_indices_.push_back(predicates_.size() - 1);

    // Always-false predicates
    predicates_.push_back({
        "square_negative",
        PredicateType::AlwaysFalse,
        "x * x < 0",
        [this](const std::string& x, const std::string&) {
            std::string result = nextTemp();
            return predicates::squareNegative(x, result, temp_counter_);
        }
    });
    false_indices_.push_back(predicates_.size() - 1);

    predicates_.push_back({
        "xor_self_nonzero",
        PredicateType::AlwaysFalse,
        "(x ^ x) != 0",
        [this](const std::string& x, const std::string&) {
            std::string result = nextTemp();
            return predicates::xorSelfNonZero(x, result, temp_counter_);
        }
    });
    false_indices_.push_back(predicates_.size() - 1);

    predicates_.push_back({
        "or_lt_and",
        PredicateType::AlwaysFalse,
        "(x | y) < (x & y)",
        [this](const std::string& x, const std::string& y) {
            std::string result = nextTemp();
            return predicates::orLtAnd(x, y, result, temp_counter_);
        }
    });
    false_indices_.push_back(predicates_.size() - 1);
}

inline const OpaquePredicate& OpaquePredicateLibrary::getAlwaysTrue() {
    size_t idx = true_indices_[GlobalRandom::nextInt(0, static_cast<int>(true_indices_.size()) - 1)];
    return predicates_[idx];
}

inline const OpaquePredicate& OpaquePredicateLibrary::getAlwaysFalse() {
    size_t idx = false_indices_[GlobalRandom::nextInt(0, static_cast<int>(false_indices_.size()) - 1)];
    return predicates_[idx];
}

inline const OpaquePredicate* OpaquePredicateLibrary::getByName(const std::string& name) {
    for (const auto& pred : predicates_) {
        if (pred.name == name) {
            return &pred;
        }
    }
    return nullptr;
}

inline std::pair<std::vector<std::string>, std::string>
OpaquePredicateLibrary::generateAlwaysTrue(const std::string& var1, const std::string& var2) {
    const auto& pred = getAlwaysTrue();
    auto code = pred.llvm_generator(var1, var2);

    // Extract result variable from last instruction
    std::string result_var;
    if (!code.empty()) {
        const std::string& last = code.back();
        size_t eq_pos = last.find(" = ");
        if (eq_pos != std::string::npos) {
            size_t start = last.find_first_not_of(" \t");
            result_var = last.substr(start, eq_pos - start);
        }
    }

    return {code, result_var};
}

inline std::pair<std::vector<std::string>, std::string>
OpaquePredicateLibrary::generateAlwaysFalse(const std::string& var1, const std::string& var2) {
    const auto& pred = getAlwaysFalse();
    auto code = pred.llvm_generator(var1, var2);

    // Extract result variable from last instruction
    std::string result_var;
    if (!code.empty()) {
        const std::string& last = code.back();
        size_t eq_pos = last.find(" = ");
        if (eq_pos != std::string::npos) {
            size_t start = last.find_first_not_of(" \t");
            result_var = last.substr(start, eq_pos - start);
        }
    }

    return {code, result_var};
}

inline void OpaquePredicateLibrary::initializeContextPredicates() {
    // Loop counter predicate: (i * (i + 1)) & 1 == 0
    context_predicates_.push_back({
        "loop_counter_even",
        "(i * (i + 1)) & 1 == 0 - product of consecutive integers is always even",
        true,   // requires_loop_counter
        false,  // requires_array_size
        false,  // requires_two_variables
        [this](const ContextPredicateInfo& ctx, int& counter) {
            std::string result = nextTemp();
            auto code = predicates::loopCounterEven(ctx, result, counter);
            return std::make_pair(code, result);
        },
        nullptr
    });

    // Array size non-negative predicate
    context_predicates_.push_back({
        "array_size_nonneg",
        "size >= 0 - array sizes are always non-negative",
        false,  // requires_loop_counter
        true,   // requires_array_size
        false,  // requires_two_variables
        [this](const ContextPredicateInfo& ctx, int& counter) {
            std::string result = nextTemp();
            auto code = predicates::arraySizeNonNegative(ctx, result, counter);
            return std::make_pair(code, result);
        },
        nullptr
    });

    // AND NOT self predicate: (x & ~x) == 0
    context_predicates_.push_back({
        "and_not_self",
        "(x & ~x) == 0 - x AND (NOT x) is always zero",
        false,  // works with any variable
        false,
        false,
        [this](const ContextPredicateInfo& ctx, int& counter) {
            std::string result = nextTemp();
            auto code = predicates::andNotSelf(ctx, result, counter);
            return std::make_pair(code, result);
        },
        nullptr
    });

    // Loop bound check: uses both loop counter and bound
    context_predicates_.push_back({
        "loop_bound_check",
        "(i < n) == ((i - n) < 0) - consistency check for loop bounds",
        true,   // requires_loop_counter
        false,
        true,   // requires_two_variables
        nullptr,
        [this](const ContextPredicateInfo& counter,
               const ContextPredicateInfo& bound, int& temp_counter) {
            std::string result = nextTemp();
            auto code = predicates::loopBoundCheck(counter, bound, result, temp_counter);
            return std::make_pair(code, result);
        }
    });
}

inline const ContextOpaquePredicate* OpaquePredicateLibrary::getContextPredicate(
    bool have_loop_counter, bool have_array_size, bool have_two_vars) {

    std::vector<size_t> suitable;

    for (size_t i = 0; i < context_predicates_.size(); i++) {
        const auto& pred = context_predicates_[i];

        // Check if predicate requirements are met
        if (pred.requires_loop_counter && !have_loop_counter) continue;
        if (pred.requires_array_size && !have_array_size) continue;
        if (pred.requires_two_variables && !have_two_vars) continue;

        suitable.push_back(i);
    }

    if (suitable.empty()) {
        return nullptr;
    }

    size_t idx = suitable[GlobalRandom::nextInt(0, static_cast<int>(suitable.size()) - 1)];
    return &context_predicates_[idx];
}

inline std::pair<std::vector<std::string>, std::string>
OpaquePredicateLibrary::generateContextPredicate(const ContextPredicateInfo& ctx) {
    // Find a suitable single-variable predicate
    const auto* pred = getContextPredicate(ctx.is_loop_counter, ctx.is_array_size, false);

    if (!pred || !pred->generator) {
        // Fallback to and_not_self which works with any variable
        std::string result = nextTemp();
        auto code = predicates::andNotSelf(ctx, result, temp_counter_);
        return {code, result};
    }

    return pred->generator(ctx, temp_counter_);
}

inline std::pair<std::vector<std::string>, std::string>
OpaquePredicateLibrary::generateContextPredicate(
    const ContextPredicateInfo& ctx1,
    const ContextPredicateInfo& ctx2) {

    // Find a suitable dual-variable predicate
    const auto* pred = getContextPredicate(
        ctx1.is_loop_counter || ctx2.is_loop_counter,
        ctx1.is_array_size || ctx2.is_array_size,
        true);

    if (!pred || !pred->dual_generator) {
        // Fallback to generating two single-variable predicates combined
        std::string result1 = nextTemp();
        std::string result2 = nextTemp();
        std::string final_result = nextTemp();

        auto code1 = predicates::andNotSelf(ctx1, result1, temp_counter_);
        auto code2 = predicates::andNotSelf(ctx2, result2, temp_counter_);

        std::vector<std::string> code;
        code.insert(code.end(), code1.begin(), code1.end());
        code.insert(code.end(), code2.begin(), code2.end());
        // AND the results (both true -> true)
        code.push_back("  " + final_result + " = and i1 " + result1 + ", " + result2);

        return {code, final_result};
    }

    return pred->dual_generator(ctx1, ctx2, temp_counter_);
}

} // namespace cff
} // namespace morphect

#endif // MORPHECT_OPAQUE_PREDICATES_HPP
