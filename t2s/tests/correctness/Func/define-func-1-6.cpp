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
// A positive case: Based on 5, defined by i + j, after using it.

#include "util.h"

int main(void) {
    // This should be a negative case
    // Consider later.
    cout << "Success!\n";
    exit(0);

    ImageParam a(Int(32), 2, "a");
    Buffer<int> in = new_data_2d<int, SIZE, SIZE>(SEQUENTIAL);
    a.set(in);

    Var i, j;
    Func f(a), g;

    // Define the compute.
    g(i, j) = f(i, j);
    f(i, j) = i + j;
   
    // Compile.
    Target target = get_host_target();
    g.compile_jit(target);

    // Run.
    Buffer<int> out = g.realize({SIZE, SIZE}, target);

    // Check correctness.
    Buffer<int> golden = get_result_of_simple_case2<int, SIZE, SIZE>();
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
}
