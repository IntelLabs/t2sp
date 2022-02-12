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
#ifndef T2S_MINIMIZE_SHREGS_H
#define T2S_MINIMIZE_SHREGS_H

/** \file
 *
 * Defines a pass to minimize the sizes of shift registers.
 *
 */

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

// A register allocation decision for a variable.
struct ShiftRegAlloc {
    Type                   type;                    // Type of a register.
    vector<Expr>           args;                    // Args of a (unique) write access to the variable.

    // Dimensions of the shift registers.
    int                    vectorized_dim;          // Index to the vectorized loop. -1 if there is no vectorized loop
    bool                   vectorized_dim_as_space; // True if the vectorized_dim is an independent space dimension. This
                                                    // happens when all dependences at this dimension have a distance of 0.
                                                    // Otherwise, the vectorized_dim will be part of the time_dims.
    vector<vector<int>>    linearized_dims;         // Zero_dims_* and time_dims.
    vector<int>            PE_dims;                 // Indices of the unrolled loops in the args.

    // (Linearized) extents of the dimensions.
    Expr                   vectorized_extent;       // Extent of the vectorized_dim
    vector<Expr>           linearized_extents;      // Linearized extents of the linearized_dims.
    vector<Expr>           PE_extents;              // Extents of the PE_dims.

    // Additional info for the linearized_dims
    vector<bool>           rotate;                  // Should rotation be used for the corresponding linearized time_dims?
};

/* Minimize the sizes of shift registers. */
extern Stmt minimize_shift_registers(Stmt s, const map<string, Function> &env, map<string, ShiftRegAlloc> &func_to_regalloc);

}
}

#endif
