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
// A positive test: vectorize two loops.

#include "util.h"

#define OI  2
#define II  4
#define III 4

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 1, "a");
    Func A(PLACE0);
    Var oi, ii, iii;
    A(iii, ii, oi) = a(iii + III * ii + III * II * oi);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> in = new_data<int, III * II * OI>(RANDOM);
    a.set(in);
    Buffer<int> golden = A.realize({III, II, OI}, target);

    // Vectorize. Re-compile and run.
    A.vectorize(iii);
    A.vectorize(ii);
    remove(getenv("BITSTREAM"));
    Buffer<int> out = A.realize({III, II, OI}, target);

    // Check correctness.
    check_equal_3D<int>(golden, out);
    cout << "Success!\n";
}


