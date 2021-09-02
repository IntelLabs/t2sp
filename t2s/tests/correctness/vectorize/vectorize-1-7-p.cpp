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
// A positive test: vectorize a complex case with casts and intrincs.

#include "util.h"

#define SIZE 32
#define II   8
#define JJ   4

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 2, "a");
    Var i, oi, ii;
    Func A(Int(32), {i}, PLACE0);
    A(i) = a(cast<int>(i / JJ), cast<int>(mod_round_to_zero(i, JJ)));
    A.split(i, oi, ii, II);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> in = new_matrix<int, SIZE / JJ, JJ>(RANDOM);
    a.set(in);
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


