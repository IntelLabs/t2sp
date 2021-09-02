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
#ifndef T2S_PLACE_H
#define T2S_PLACE_H

/** \file
 *
 * Defines a pass to reflect the Place of a Func on the IR.
 *
 */

#include "../../Halide/src/IR.h"
#include "ComputeLoopBounds.h"
#include "SpaceTimeTransform.h"

using std::vector;
using std::map;
using std::string;
using std::set;

namespace Halide {
namespace Internal {


/* Wrap the Producer body of a Func (which must have only one and only Definition) with a unit-trip-count
 * loop that is marked a postfix ".run_on_device", which tells CodeGen_LLVM and CodeGen_GPU_Host to offload
 * the entire loop body to the device.
 */
extern Stmt place_device_functions(Stmt s, const std::map<std::string, Function> &env, const Target &t);

/* Make a producer and consumer Func, both on the device, communicate through a channel.
 */
extern Stmt replace_references_with_channels(Stmt s, const std::map<std::string, Function> &env);

/* Make a producer and consumer Func, both on the device, communicate through a channel.
   The channel depth will be given by the global bounds.
 */
extern Stmt replace_references_with_channels(Stmt s, const std::map<std::string, Function> &env,
                                             const LoopBounds &global_bounds);

extern Stmt replace_references_with_mem_channels(Stmt s, const std::map<std::string, Function> &env,
                                                 map<string, Place> &funcs_using_mem_channels);

/* Replace accesses to shift registers, so far as common memory, in write/read_shift_reg instead. */
extern Stmt replace_references_with_shift_registers(Stmt s, const map<string, Function> &env,
                                                    const std::map<std::string, RegBound > &reg_size_map);

}
}

#endif
