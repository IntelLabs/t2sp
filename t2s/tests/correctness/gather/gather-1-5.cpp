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
// A positive test for gathering, 1-D gathering

#include "util.h"

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 1, "a");
    Func A(Place::Device);
    Var i;
    A(i) = a(i);

    // Target.
    Target target = get_host_target().with_feature(Target::IntelFPGA);

    // Generate input.
    Buffer<int> in = new_data<int, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(in);

    // Set bounds, isolate.
    Func A_drainer(Place::Device), A_collector(Place::Device);
    A.set_bounds(i, 0, SIZE)
     .vectorize(i)
     .isolate_consumer_chain(A_drainer, A_collector);

    // Gather.
    A_drainer.gather(A, i, STRATEGY);

    // Compile and run.
    A_collector.compile_jit(target);
    Buffer<int> out = A_collector.realize(SIZE, target);

    // Golden.
    Func A0(Place::Host);
    Var i0;
    A0(i0) = a(i0);
    A0.compile_jit(target);
    Buffer<int> golden = A0.realize(SIZE, target);
    // Check correctness.
    check_equal<int>(golden, out);
    cout << "Success!\n";
}
