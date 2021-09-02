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
// A positive test case: long merge_ures

#include "util.h"

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 2, "a");
    ImageParam b(Int(32), 2, "b");
    Var i, j, k;
    Func A(Int(32), {i, j, k}), B(Int(32), {i, j, k}), C(Int(32), {i, j, k});
    Func D(Int(32), {i, j, k}), E(Int(32), {i, j, k}), F(Int(32), {i, j, k});
    Func G(Int(32), {i, j, k}), H(Int(32), {i, j, k}), I(Int(32), {i, j, k});
    Func J(Int(32), {i, j, k}), K(Int(32), {i, j, k}), L(Int(32), {i, j, k});
    Func M(Int(32), {i, j, k}), N(Int(32), {i, j, k});

    A(i, j, k) = select(j == 0, a(i, k), A(i, j - 1, k));
    B(i, j, k) = select(i == 0, b(k, j), B(i - 1, j, k));
    C(i, j, k) = select(k == 0, A(i, j, k) * B(i, j, k), C(i, j, k - 1) + A(i, j, k) * B(i, j, k));
    D(i, j, k) = i + j;
    E(i, j, k) = a(i, j) + b(j, k);
    F(i, j, k) = - D(i, j, k) + E(i, j, k);
    G(i, j, k) = C(i, j, k) + i;
    H(i, j, k) = G(i, j, k) - i;
    I(i, j, k) = A(i, j, k) + 1;
    J(i, j, k) = I(i, j, k) - I(i, j, k);
    K(i, j, k) = J(i, j, k) / 2;
    L(i, j, k) = H(i, j, k) + K(i, j, k);
    M(i, j, k) = 0 * F(i, j, k) + L(i, j, k);
    N(i, j, k) = (L(i, j, k) + M(i, j, k)) / 2;
    A.merge_ures(B, C, D, E, F, G, H, I, J, K, L, M, N);

    // Compile.
    Target target = get_host_target();
    N.compile_jit(target);

    // Run.
    Buffer<int> ina = new_data_2d<int, SIZE, SIZE>(SEQUENTIAL); //or RANDOM
    Buffer<int> inb = new_data_2d<int, SIZE, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Buffer<int> out = extract_result_of_mm<int, SIZE, SIZE, SIZE>(N.realize({SIZE, SIZE, SIZE}, target));             

    // Check correctness.
    Buffer<int> golden = get_result_of_mm<int, SIZE, SIZE, SIZE>(ina, inb);
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
}
