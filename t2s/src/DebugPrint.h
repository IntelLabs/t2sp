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
#ifndef T2S_DEBUG_PRINT_H
#define T2S_DEBUG_PRINT_H

#include <algorithm>
#include <iostream>
#include <string.h>
#include <fstream>
#include "../../Halide/src/Bounds.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/ImageParam.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IROperator.h"
#include "../../Halide/src/IRPrinter.h"
#include "../../Halide/src/Schedule.h"
#include "../../Halide/src/Util.h"
#include "./SliceExprTree.h"
#include "./StructType.h"

namespace Halide {
using std::map;
using std::string;
using std::vector;
using std::set;
using std::pair;
using std::ofstream;
using std::tuple;

namespace Internal {
string tabs(size_t tab);
string to_string(const string &str);
string to_string(const Expr &expr);
string to_string(const Stmt &stmt);
string to_string(const Var &var);
string to_string(const ReductionVariable &var);
string to_string(const Split &split);
string to_string(const Dim &dim);
string to_string(const LoopLevel &level);
string to_string(const FusedPair &pair);
string to_string(const SliceExprTree &tree);
string to_string(const Type &type);
string to_string(const Func &f, bool defs_only = false, bool print_merged_ures = false);
string to_string(const Function &function, bool defs_only = false, bool print_merged_ures = false);
string to_string(const string &name, const vector<Type> &output_types, const Definition &definition, size_t tab = 0, bool defs_only = false, bool print_merged_ures = false);
string to_string(const StageSchedule &schedule, size_t tab = 0);
string to_string(const Region &bounds);
string to_string(const vector<tuple<string, Expr, bool>> &loop_info, bool dont_print_prefix);

template<typename T>
string to_string(const vector<T> &v, bool separate = true) {
    std::ostringstream s;
    for (size_t i = 0; i < v.size(); i++) {
        s << (separate ? ((i==0) ? "" : ", ") : "") << to_string(v[i]);
    }
    return s.str();
}

template<typename T>
string to_string(const set<T> &v, bool separate = true) {
    std::ostringstream s;
    size_t i = 0;
    for (auto item : v) {
        s << (separate ? ((i==0) ? "" : ", ") : "") << to_string(item);
        i++;
    }
    return s.str();
}

string names_to_string(const vector<Func> &v);
string names_to_string(const vector<VarOrRVar> &v);
string names_to_string(const vector<Var> &v);
string names_to_string(const vector<ImageParam> &im);
string to_string(const map<string, Box> &boxes);
}
}
#endif
