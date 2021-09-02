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
// A positive test: vectorize a filter.

#include "util.h"

#define I  16
#define J  8
#define JJ 4

int main(void) {
    // Define the compute.
    ImageParam b(Int(32), 2, "b"), w(Int(32), 2, "w");
    Var i, j, oj, jj;
    Func A(Int(32), {i, j}, PLACE0), B(Int(32), {i, j}, PLACE1), W(Int(32), {i, j}, PLACE2);
    B(i, j) = b(i, j);
    W(i, j) = w(i, j);
    A(i, j) = select(i == 0, B(i, j) * W(i, j),
                select(j == 0, A(i-1, j + J - 1) + B(i, j) * W(i, j),
                                A(i, j-1) + B(i, j) * W(i, j)));
    B.compute_root();
    W.compute_root();
    A.reorder(j, i)
     .split(j, oj, jj, JJ);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> inb = new_matrix<int, I, J>(RANDOM);
    Buffer<int> inw = new_matrix<int, I, J>(RANDOM);
    b.set(inb);
    w.set(inw);
    Buffer<int> golden = A.realize({I, J}, target);

    // Vectorize. Re-compile and run.
    A.vectorize(jj);
    remove(getenv("BITSTREAM"));
    Buffer<int> out = A.realize({I, J}, target);

    // Check correctness.
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
    return 0;
}


