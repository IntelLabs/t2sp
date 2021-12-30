/*******************************************************************************
* Copyright 2021 Intel Corporation
*
* Licensed under the BSD-2-Clause Plus Patent License (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSDplusPatent
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: BSD-2-Clause-Patent
*******************************************************************************/
#include "./Utilities.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IROperator.h"
#include <queue>

namespace Halide {
namespace Internal {
using std::string;
using std::map;
using std::vector;

bool function_is_in_environment(const string &func_name, const map<string, Function> &env, Function &func) {
    if (env.find(func_name) != env.end()) {
        func = env.at(func_name);
        return true;
    }
    return false;
}

string extract_first_token(const string &str) {
    size_t i;
    for (i = 0; i < str.size() && str[i] != '.'; i++) { ; }
    return str.substr(0, i);    
}

string extract_last_token(const string &str) {
    int i;
    for (i = str.size() - 1; i >= 0 && str[i] != '.'; i--) { ; }
    return str.substr(i + 1, str.size() - i - 1);
}

string extract_token(const string &str, int num_tokens) {
    size_t i;
    size_t j = 0;
    int tokens = 0;
    for (i = 0; i < str.size(); i++) {
        if (str[i] == '.') {
            tokens++;
            if (tokens == num_tokens) {
                return str.substr(j, i-j);
            }
            j = i + 1;
        }
    }
    if (tokens +1 == num_tokens)
        return str.substr(j, i-j);
    return "";
}

string extract_before_tokens(const string &str, int num_tokens) {
    size_t i;
    int tokens = 0;
    for (i = 0; i < str.size(); i++) {
        if (str[i] == '.') {
            tokens++;
            if (tokens == num_tokens) {
                return str.substr(0, i);
            }
        }
    }
    return "";
}

string extract_after_tokens(const string &str, int num_tokens) {
    size_t i;
    int tokens = 0;
    for (i = 0; i < str.size(); i++) {
        if (str[i] == '.') {
            tokens++;
            if (tokens == num_tokens) {
                return str.substr(i + 1, str.size() - i - 1);
            }
        }
    }
    return "";
}

string remove_postfix(const string &str, const string &postfix) {
    internal_assert(ends_with(str, postfix));
    return str.substr(0, str.size() - postfix.size());
}

Region loop_region(const vector<const For *> &loops, bool bounds_must_be_const) {
    Region region;
    for (auto l : loops) {
        if (bounds_must_be_const) {
            internal_assert(is_const(l->min));
            internal_assert(is_const(l->extent));
        }
        region.push_back(Range(l->min, l->extent));
    }
    return std::move(region);
}

vector<Expr> loop_extents(const vector<const For *> &loops, bool extents_must_be_const) {
    vector<Expr> extents;
    for (auto l : loops) {
        if (extents_must_be_const) {
            internal_assert(is_const(l->extent));
        }
        extents.push_back(l->extent);
    }
    return std::move(extents);
}


vector<Expr> loop_indices(const vector<const For *> &loops) {
    vector<Expr> indices;
    for (auto l : loops) {
        indices.push_back(Variable::make(Int(32), l->name));
    }
    return std::move(indices);
}

vector<Expr> break_logic_into_conjunction(const Expr &cond) {
    vector<Expr> results;
    std::queue<Expr>  terms;
    terms.push(cond);
    while (!terms.empty()) {
        Expr term = terms.front();
        terms.pop();
        if (term.as<And>()) {
           const And *t = term.as<And>();
           terms.push(t->a);
           terms.push(t->b);
        } else {
            results.push_back(term);
        }
    }
    return std::move(results);
}

bool check_is_single_PE(bool on_device, const Expr &cond, const vector<string> &unrolled_loops,
                        const vector<string> &loops_to_serialize,
                        Expr &single_PE_cond, vector<string> &unrolled_loops_without_terms) {
    single_PE_cond = const_true();
    unrolled_loops_without_terms = unrolled_loops;
    if (!on_device) {
        // On the host, even if we unroll loops, that won't create multiple physical PEs.
        return true;
    }
    vector<Expr> conjuction = break_logic_into_conjunction(cond);
    for (auto e : conjuction) {
        // Look for unroll loop var == a constant
        const EQ * eq = e.as<EQ>();
        if (eq != NULL) {
            if (is_const(eq->b)) {
                if (eq->a.as<Variable>() != NULL) {
                    string name = eq->a.as<Variable>()->name;
                    auto itr = std::find(unrolled_loops_without_terms.begin(), unrolled_loops_without_terms.end(), name);
                    if (itr != unrolled_loops_without_terms.end()) {
                        unrolled_loops_without_terms.erase(itr);
                        single_PE_cond = equal(single_PE_cond, const_true()) ? e : single_PE_cond && e;
                    }
                }
            }
        }
    }
    if (unrolled_loops_without_terms.size() > 0) {
        for (size_t i = 0; i < unrolled_loops_without_terms.size(); i++) {
            string u = unrolled_loops_without_terms[i];
            auto itr = std::find(loops_to_serialize.begin(), loops_to_serialize.end(), u);
            if (itr != loops_to_serialize.end()) {
                // This loop should be counted as serial.
                unrolled_loops_without_terms.erase(unrolled_loops_without_terms.begin() + i);
            }
        }
    }
    if (unrolled_loops_without_terms.size() == 0) {
        return true;
    }
    return false;
}

// Loop variable is a constant
bool loop_var_is_constant_in_condition(const string &loop_name, const Expr &cond) {
    vector<Expr> conjuction = break_logic_into_conjunction(cond);
    Expr value; // Value found for the loop var from the condition. Initially undefined.
    for (auto e : conjuction) {
        // Look for loop var == a constant
        const EQ * eq = e.as<EQ>();
        if (eq != NULL) {
            if (is_const(eq->b)) {
                if (eq->a.as<Variable>() != NULL) {
                    string name = eq->a.as<Variable>()->name;
                    if (name == loop_name) {
                        if (value.defined()) {
                            user_assert(equal(value, eq->b)) << "In condition " << cond
                                    << ", loop " << loop_name << " has two different values: "
                                    << value << ", and " << eq->b << "\n";
                        } else {
                            value = eq->b;
                        }
                    }
                }
            }
        }
    }
    if (value.defined()) {
        return true;
    }
    return false;
}

bool is_power_of_two(uint32_t n)
{
    internal_assert(n != 0);
    return (!(n & (n - 1)));
}

uint32_t closest_power_of_two(uint32_t n)
{
    internal_assert(n != 0);
    if (is_power_of_two(n)) {
        return n;
    }
    uint32_t count = 0;
    while( n != 0) {
        n >>= 1;
        count += 1;
    }
    return 1 << count;
}


namespace {
class ReplacePrefix: public IRMutator {
    using IRMutator::visit;
    const string &old_prefix;
    const string &new_prefix;
public:
    ReplacePrefix(const string &old_prefix, const string &new_prefix) :
        old_prefix(old_prefix), new_prefix(new_prefix) { }

    Expr visit(const Variable *v) override {
        if (starts_with(v->name, old_prefix)) {
            string new_name = new_prefix + v->name.substr(old_prefix.size(), v->name.size() - old_prefix.size());
            Expr new_expr = Variable::make(v->type, new_name, v->image, v->param, v->reduction_domain);
            return new_expr;
        }
        return v;
    }
};

} // namespace

Expr replace_prefix(const string &old_prefix, const string &new_prefix, const Expr &e) {
    Expr new_value = ReplacePrefix(old_prefix, new_prefix).mutate(e);
    return new_value;
}

}
}
