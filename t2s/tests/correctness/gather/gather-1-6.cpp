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
// A positive test for gathering, 3-D gathering

#include "util.h"

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 2, "a");
    Func A(Place::Device), B(Place::Device);
    Var i, j, k;
    A(i, j, k) = a(i, j) + i;
    B(i, j, k) = A(i, j, k) * j;

    // Target.
    Target target = get_host_target().with_feature(Target::IntelFPGA);

    // Generate input.
    Buffer<int> in = new_matrix<int, SIZE, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(in);

    // Isolate.
    Func B_drainer(Place::Device), B_collector(Place::Device);
    B.set_bounds(i, 0, SIZE,
                 j, 0, SIZE,
                 k, 0, SIZE)
     .unroll(j).vectorize(i)
     .isolate_consumer_chain(B_drainer, B_collector);

    // Gather.
    B_drainer.gather(B, i, STRATEGY);

    // Compile and run.
    B_collector.compile_jit(target);
    Buffer<int> out = B_collector.realize(SIZE, SIZE, SIZE, target);

    // Golden.
    Func A0(Place::Host), B0(Place::Host);
    Var i0, j0, k0;
    A0(i0, j0, k0) = a(i0, j0) + i0;
    B0(i0, j0, k0) = A(i0, j0, k0) * j0;
    A0.compile_jit(target);
    Buffer<int> golden = B0.realize(SIZE, SIZE, SIZE, target);  

    // for(int t1 = 0;t1 < SIZE;++t1){
    //     for(int t2 = 0;t2 < SIZE;++t2){
    //         for(int t3 = 0; t3 < SIZE; ++t3){
    //             std::cout<<"["<<t1<<", "<<t2<<", "<<t3<<"] : output = "<<out(t1,t2,t3) <<", golden = "<< golden(t1,t2,t3)<<"\n";
    //         }
    //     }
    // }

    // Check correctness.
    check_equal_3D<int>(golden, out);
    cout << "Success!\n";
}
