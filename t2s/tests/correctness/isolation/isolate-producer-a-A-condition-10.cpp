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
#include "util.h"

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 1, "a");
    Func A(PLACE0), B;
    Var i;
    A(i) = select(i <= 2, a(i) + 1, i * 2) * 2;
    B(i) = select(i <= 2, a(i) + 1, i * 2) * 2;

    // Compile.
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> in = new_data<int, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(in);
    Buffer<int> golden = B.realize(SIZE, target);

    // Isolate. Re-compile and run.
    Func A_feeder(PLACE1);
    std::cout << "Before isolation:\n"
              << Internal::to_string(A, true, true);
    A.isolate_producer_chain(a, A_feeder);
    std::cout << "After isolation:\n"
              << Internal::to_string(A_feeder, true, true)
              << Internal::to_string(A, true, true);

    Buffer<int> out = A.realize(SIZE, target);

    // Check correctness.
    check_equal<int>(golden, out);
    cout << "Success!\n";
}


