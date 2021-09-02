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
// A positive test: vectorize a reduction.
// TOFIX: Split after merge_ures should propagate that splitting info to all the merged UREs.

#include "util.h"

#define I  4
#define J  6
#define K  8
#define KK 4

int main(void) {
    // Define the compute.
    ImageParam b(Int(32), 2, "b");
    ImageParam c(Int(32), 2, "c");
    Var i, j, k, ok, kk;
    Func A(Int(32, 1), {i, j, k}, PLACE0), B(PLACE0), C(PLACE0);
    B(i, j, k) = b(i, k);
    C(i, j, k) = c(k, j);
    A(i, j, k) = select(k == 0, B(i, j, k) * C(i, j, k), A(i, j, k-1) + B(i, j, k) * C(i, j, k));

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> inb = new_data_2d<int, I, K>(RANDOM);
    Buffer<int> inc = new_data_2d<int, K, J>(RANDOM);
    b.set(inb);
    c.set(inc);
    Buffer<int> golden = A.realize({I, J, K}, target);

    // Vectorize. Re-compile and run.
    A.merge_ures(B, C)
     .split(k, ok, kk, KK)
     .reorder(kk, ok, j, i);
    A.vectorize(kk);
    remove(getenv("BITSTREAM"));
    Buffer<int> out = A.realize({I, J, K}, target);

    // Check correctness.
    check_equal_3D<int>(golden, out);
    cout << "Success!\n";
}


