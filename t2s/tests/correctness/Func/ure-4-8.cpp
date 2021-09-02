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
// A positive test case: Extended URE, less than 1 args.

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j, k;
    Func f, g, h;
    f(i, j, k) = 0;
    g(i, j, k) = f(i, j, k);
    h(k) = select(i == 5 && j == 5, g(i, j, k), 0);

    f.merge_ures(g, h)
     .set_bounds(j, 0, 10,
                 i, 0, 10);

    // Compile.
    Target target = get_host_target();
    h.compile_jit(target);

    // Run.
    Buffer<int> out = h.realize({SIZE}, target);

    // Check correctness.
    for (size_t i = 0; i < SIZE; i++) {
        assert(out(i) == 0);
    }

    cout << "Success!\n";
}
