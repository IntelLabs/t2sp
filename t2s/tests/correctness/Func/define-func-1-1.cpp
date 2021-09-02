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
// An positive test case: A normal define pipeline.

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j;
    Func f(Float(32, 1), {i, j}), g;
    // Will an automatic type conversion be done?
    f(i, j) = (i + j) * 1.0f;
    g(i, j) = 0*(i + j) + f(i, j - 1);

    // Compile.
    Target target = get_host_target();
    g.compile_jit(target);

    // Run.
    Buffer<float> out = g.realize({SIZE, SIZE}, target);

    // Check correctness.
    Buffer<float> golden = get_result_of_simple_case1<float, SIZE, SIZE>();
    check_equal_2D<float>(golden, out);
    cout << "Success!\n";
}
