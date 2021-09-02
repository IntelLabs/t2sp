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
// A positive test case: Merge two UREs that have no dependency.

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j;
    Func f, g, h;

    f(i, j) = 0;
    g(i, j) = 0;
    f.merge_ures(g);
    h(i, j) = g(i, j) + f(i, j);

    // Compile.
    Target target = get_host_target();
    h.compile_jit(target);
    
    // Run.
    Buffer<int> out = h.realize({SIZE, SIZE}, target);

    // Check correctness.
    for (size_t i = 0; i < SIZE; i++) {
        for (size_t j = 0; j < SIZE; j++) {
            assert(out(i, j) == 0);
        }
    }
    cout << "Success!\n";
}
