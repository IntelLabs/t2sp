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
// A positive test: two stages; only consumer is vectorized.

#include "util.h"

#define SIZE 32
#define II   8

int main(void) {
    // Define the compute.
    ImageParam b(Int(32), 1, "b");
    Func A(PLACE0), B(PLACE1);
    Var i, oi, ii;
    B(i) = b(i);
    A(i) = (B(i) + 1) * 2;
    B.compute_root();
    A.split(i, oi, ii, II);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> in = new_data<int, SIZE>(RANDOM);
    b.set(in);
    Buffer<int> golden = A.realize(SIZE, target);

    // Vectorize. Re-compile and run.
    A.vectorize(ii);
    remove(getenv("BITSTREAM"));
    Buffer<int> out = A.realize(SIZE, target);

    // Check correctness.
    check_equal<int>(golden, out);
    cout << "Success!\n";
    return 0;
}


