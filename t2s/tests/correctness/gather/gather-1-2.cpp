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
// A positive test for gathering, 2-D gathering

#include "util.h"

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 2, "a");
    Func A(Place::Device);
    Var i, j;
    A(i, j) = a(i, j);

    // Target.
    Target target = get_host_target().with_feature(Target::IntelFPGA);

    // Generate input.
    Buffer<int> in = new_matrix<int, SIZE, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(in);

    // Set bounds, isolate.
    Func A_drainer(Place::Device), A_collector(Place::Device);
    A.set_bounds(i, 0, SIZE,
                 j, 0, SIZE)
     .unroll(i).unroll(j)
     .isolate_consumer_chain(A_drainer, A_collector);

    // Gather.
    A_drainer.gather(A, j, STRATEGY);

    // Compile and run.
    A_collector.compile_jit(target);
    Buffer<int> out = A_collector.realize(SIZE, SIZE, target);

    // Golden.
    Func A0(Place::Host);
    Var i0, j0;
    A0(i0, j0) = a(i0, j0);
    A0.compile_jit(target);
    Buffer<int> golden = A0.realize(SIZE, SIZE, target);

    // Check correctness.
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
}
