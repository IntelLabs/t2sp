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
#ifndef T2S_GATHER_H
#define T2S_GATHER_H

#include "../../Halide/src/IR.h"
/** \file
 * Defines data gathering.
 */

#define GATHER_ERROR_MESSAGE(func, caller, loop) ("Failed in gathering " + (func) + " in " + (caller) + " along loop " + (loop) + ":\n")

namespace Halide {
namespace Internal{
/* Find loops that will be serialized due to gathering. If Func c is isolated out of Func p as a consumer,
 * and Func p gathers data along loop l, then in Func c, loop l will be serialized during gathering.
 */
extern void find_loops_to_serialize_by_gathering(const std::map<std::string, Function> &env, std::vector<std::string> &loops_to_serialize);


extern Stmt gather_data(Stmt s, const std::map<std::string, Function> &env);
}
}
#endif

