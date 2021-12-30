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
#include "./DebugPrint.h"
#include "./Utilities.h"

namespace Halide {
using std::map;
using std::string;
using std::vector;
using std::set;
using std::pair;
using std::ofstream;
using std::tuple;

namespace Internal {

string tabs(size_t tab) {
    std::ostringstream s;
    for (size_t i = 0; i < tab; i++) {
        s << "  ";
    }
    return s.str();
}

string to_string(const string &str) {
    return str;
}

string to_string(const Expr &expr) {
    std::ostringstream s;
    s << expr;
    return s.str();
}

string to_string(const vector<Expr> &e) {
    std::ostringstream s;
    for (size_t i = 0; i < e.size(); i++) {
        s << ((i==0) ? "" : ", ") << e[i] << "\n";
    }
    return s.str();
}

string to_string(const Stmt &stmt) {
    std::ostringstream s;
    s << stmt;
    return s.str();
}

string to_string(const Var &var) {
    std::ostringstream s;
    s << var.name();
    return s.str();
}

string to_string(const ReductionVariable &var) {
    std::ostringstream s;
    s << var.var << "(" << var.min << ", " << var.extent << ")";
    return s.str();
}

string to_string(const Split &split) {
    std::ostringstream s;
    s << split.old_var << "(" << split.outer << ", " << split.inner << ", " << split.factor << ")";
    return s.str();
}

string to_string(const Dim &dim) {
    std::ostringstream s;
    s << dim.var;
    return s.str();
}

string to_string(const LoopLevel &level) {
    return level.to_string();
}

string to_string(const FusedPair &pair) {
    return "(" + pair.func_1 + " " + std::to_string(pair.stage_1) +
           ", " + pair.func_2 + " " + std::to_string(pair.stage_2) +
           ", "+ pair.var_name + ")";
}

string to_string(const SliceExprTree &tree) {
    return tree.to_string();
}

string to_string(const Type &type) {
    std::ostringstream s;
    s << type;
    return s.str();
}

string to_string(const Func &func, bool defs_only, bool print_merged_ures) {
    return to_string(func.function(), defs_only, print_merged_ures);
}

string to_string(const Function &function, bool defs_only, bool print_merged_ures) {
    std::ostringstream s;
    s << "Func " << function.name() << " at Place " << (function.place() == Place::Host ? "Host" : "Device") << ":\n";
    const Definition &initial_def = function.definition();
    s << to_string(function.name(), function.output_types(), initial_def, 1, defs_only, print_merged_ures);
    const std::vector<Definition> &updates = function.updates();
    for (auto u : updates) {
        s << to_string(function.name(), function.output_types(), u, 1, defs_only, print_merged_ures);
    }
    return s.str();
}

string to_string(const string &name, const vector<Type> &output_types,
                 const Definition &definition,
                 size_t tab, bool defs_only, bool print_merged_ures) {
    std::ostringstream s;
    if (print_merged_ures) {
        const vector<Func> &merged_ures = definition.schedule().merged_ures();
        for (auto f : merged_ures) {
            s << to_string(f, defs_only, print_merged_ures);
        }
    }
    for (auto t : output_types) {
        s << to_string(t) << " ";
    }
    s << tabs(tab) << name << "(" << to_string(definition.args()) << ") = "
            << to_string(definition.values()) << " ";
    if (definition.predicate().defined()) {
        if (equal(const_true(), definition.predicate())) {
            // Not predicated. No need to print.
        } else {
            s << to_string(definition.predicate());
        }
    }
    s << "\n";
    if (!defs_only) {
        s << to_string(definition.schedule(), tab + 1);
    }
    // TODO: specializations.
    return s.str();
}

string to_string(const StageSchedule &schedule, size_t tab) {
    std::ostringstream s;
    s << tabs(tab) << "RVars: " << to_string(schedule.rvars()) << "\n";
    s << tabs(tab) << "Splits: " << to_string(schedule.splits()) << "\n";
    s << tabs(tab) << "Dims: " << to_string(schedule.dims()) << "\n";
    s << tabs(tab) << "Fuse level: " << to_string(schedule.fuse_level().level) << "\n";
    s << tabs(tab) << "Fuse pairs: " << to_string(schedule.fused_pairs()) << "\n";
    s << tabs(tab) << "Merged ures: " << names_to_string(schedule.merged_ures()) << "\n";
    return s.str();
}

string to_string(const vector<tuple<string, Expr, bool>> &loop_info, bool dont_print_prefix) {
    std::ostringstream s;
    for (auto &l : loop_info) {
        string name = std::get<0>(l);
        if (dont_print_prefix) {
            name = extract_after_tokens(name, 2);
        }
        s << name << (std::get<2>(l) ? "(unrolled, extent=" + to_string(std::get<1>(l)) + ")  " : "  ");
    }
    return s.str();
}


string names_to_string(const vector<Func> &v) {
    std::ostringstream s;
    for (size_t i = 0; i < v.size(); i++) {
        s << ((i==0) ? "" : ", ") << v[i].name();
    }
    return s.str();
}

string names_to_string(const vector<VarOrRVar> &v) {
    std::ostringstream s;
    for (size_t i = 0; i < v.size(); i++) {
        s << ((i==0) ? "" : ", ") << v[i].name();
    }
    return s.str();
}

string names_to_string(const vector<Var> &v) {
    std::ostringstream s;
    for (size_t i = 0; i < v.size(); i++) {
        s << ((i==0) ? "" : ", ") << v[i].name();
    }
    return s.str();
}

string names_to_string(const vector<ImageParam> &im) {
    std::ostringstream s;
    for (size_t i = 0; i < im.size(); i++) {
        s << ((i==0) ? "" : ", ") << im[i].name();
    }
    return s.str();
}

string to_string(const map<string, Box> &boxes) {
    std::ostringstream s;
    for (auto b: boxes) {
        s << b.first << " (" << b.second << ")\n";
    }
    return s.str();
}

string to_string(const Region &bounds) {
    std::ostringstream s;
    for (auto b: bounds) {
        s << " [" << b.min << ", " << b.extent << "]";
    }
    return s.str();
}

}
}
