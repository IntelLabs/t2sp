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
#ifndef T2S__FLATTEN_LOOPS_H
#define T2S__FLATTEN_LOOPS_H

/** \file
 * Methods for finding use-def chains halide statements and expressions
 */

#include "../../Halide/src/IR.h"

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace Halide {
namespace Internal {

Stmt replace_mem_channels(Stmt s, const std::map<std::string, Function> &env, vector<std::pair<string, Expr>> &letstmts_backup);
Stmt flatten_loops(Stmt s, const std::map<std::string, Function> &env);
Stmt flatten_outer_loops(Stmt s, string &loop_lvl, const std::map<std::string, Function> &env);

}
}

#endif
