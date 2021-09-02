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
// A positive test case: f(i, j) = select(i == 0, 0, select(i == 1, f(i - 1, j), f(i - 2, j)))

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j;
    Func f(Int(32), {i, j});

    f(i, j) = select(i == 0, 1,
        select(i == 1, f(i - 1, j), f(i - 2, j)));

    Target target = get_host_target();
    f.compile_jit(target);
    
    // Run.
    Buffer<int> out = f.realize({SIZE, SIZE}, target);

    // Check correctness.
    for (size_t i = 0; i < SIZE; i++) {
        for (size_t j = 0; j < SIZE; j++) {
            assert(out(i, j) == 1);
        }
    }

    cout << "Success!\n";
}
