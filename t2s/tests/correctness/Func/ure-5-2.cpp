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
// A positive test case: Partial merge, C.merge_ures(A), B should do compute_root() as needed.

#include "util.h"

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 2, "a");
    ImageParam b(Int(32), 2, "b");
    Var i, j, k;
    Func A(Int(32), {i, j, k}), B(Int(32), {i, j, k}), C(Int(32), {i, j, k});

    A(i, j, k) = select(j == 0, a(i, k), A(i, j - 1, k));
    B(i, j, k) = select(i == 0, b(k, j), B(i - 1, j, k));
    C(i, j, k) = select(k == 0, A(i, j, k) * B(i, j, k), C(i, j, k - 1) + A(i, j, k) * B(i, j, k));
    B.compute_root();
    A.merge_ures(C);

    // Compile.
    Target target = get_host_target();
    C.compile_jit(target);

    // Run.
    Buffer<int> ina = new_data_2d<int, SIZE, SIZE>(SEQUENTIAL); //or RANDOM
    Buffer<int> inb = new_data_2d<int, SIZE, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Buffer<int> out = extract_result_of_mm<int, SIZE, SIZE, SIZE>(C.realize({SIZE, SIZE, SIZE}, target));             

    // Check correctness.
    Buffer<int> golden = get_result_of_mm<int, SIZE, SIZE, SIZE>(ina, inb);
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
}
