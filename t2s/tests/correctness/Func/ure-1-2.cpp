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
// A positive test case: A simple case

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j;
    Func f, g, h;
    f(i, j) = i + j - 1;
    g(i, j) = f(i, j);
    h(i, j) = select(g(i, j) > 0, f(i, j), g(i, j));

    f.merge_ures(g, h);

    // Compile.
    Target target = get_host_target();
    h.compile_jit(target);

    // Run.
    Buffer<int> out = h.realize({SIZE, SIZE}, target);

    // Check correctness.
    Buffer<int> golden = get_result_of_simple_case1<int, SIZE, SIZE>();
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
}
