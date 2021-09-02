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
#ifndef T2S_BUILD_CALL_RELATION_H
#define T2S_BUILD_CALL_RELATION_H

/** \file
 *
 * Defines a pass to build a call graph.
 *
 */

#include "../../Halide/src/Function.h"
using std::map;
using std::string;
using std::vector;

namespace Halide {
namespace Internal {

/* Build a map where a merged URE is mapped to its representative URE, and other functions are mapped to themselves.
 */
extern map<string, string> map_func_to_representative(const map<string, Function> &env);

/* Build a map from the name of each Halide Function to the names of the Halide Functions it calls.
 * If use_representative_for_merged_ures is true, merged UREs will be represented by their representative Function.
 * If ignore_self_calls is true, self calls of functions are ignored.
 */
extern map<string, vector<string>> build_call_graph(const map<string, Function> &env,
        bool use_representative_for_merged_ures = false, bool ignore_self_calls = false);

/* Build a map from the name of each Halide Function to the names of the Halide Functions calling it.
 */
extern map<string, vector<string>> build_reverse_call_graph(const map<string, vector<string>> &call_graph);

}
}

#endif
