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
// A positive test case: Declared by UInt(32, 1), {i, j}

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j;
    Func f(UInt(32, 1), {i, j}), g;
    g(i, j) = f(i, j - 1);
    f(i, j) = cast(UInt(32, 1), i + j);

    // Compile.
    Target target = get_host_target();
    g.compile_jit(target);

    // Run.
    Buffer<unsigned> out = g.realize({SIZE, SIZE}, target);

    // Check correctness.
    Buffer<unsigned> golden = get_result_of_simple_case1<unsigned, SIZE, SIZE>();
    check_equal_2D<unsigned>(golden, out);
    cout << "Success!\n";
}
