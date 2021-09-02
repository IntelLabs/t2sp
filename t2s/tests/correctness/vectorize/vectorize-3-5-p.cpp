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
// A positive test: producer and consumer's loop have different sizes.

#include "util.h"

#define SIZE1  2
#define SIZE2  32
#define II     8
#define III    4

int main(void) {
    // Define the compute.
    ImageParam b(Int(32), 2, "b");
    Func A(PLACE0), B(PLACE1);
    Var i, ii, oii, iii, oiii, iiii;
    B(i, ii) = b(i, ii);
    A(i, ii) = B(i, ii);
    B.compute_root();
    A.reorder(ii, i).split(ii, oii,  iii,  II);
    B.reorder(ii, i).split(ii, oiii, iiii, III);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> in = new_matrix<int, SIZE1, SIZE2>(RANDOM);
    b.set(in);
    Buffer<int> golden = A.realize({SIZE1, SIZE2}, target);

    // Vectorize. Re-compile and run.
    A.vectorize(iii);
    B.vectorize(iiii);
    remove(getenv("BITSTREAM"));
    Buffer<int> out = A.realize({SIZE1, SIZE2}, target);

    // Check correctness.
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
    return 0;
}


