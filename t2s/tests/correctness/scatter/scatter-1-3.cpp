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
    ImageParam a(Int(32), 3, "a");
    Func A(Place::Device), Afeeder(Place::Device);
    Var i, j, k;
    A(i, j, k) = a(i, j, k) * 2;
    A.set_bounds(i,0,SIZE,
                 j,0,SIZE,
                 k,0,SIZE);
    A.space_time_transform(i,j).vectorize(i);
    A.isolate_producer_chain(a, Afeeder);
    Afeeder.scatter(a,j,STRATEGY);
    Buffer<int> in = new_data_3D<int, SIZE,SIZE,SIZE>(SEQUENTIAL);
    a.set(in);
    Target target1 = get_host_target();
    target1.set_feature(Target::IntelFPGA);
    Buffer<int> out = A.realize(SIZE,target1);

    Func A0(Place::Host);
    A0(i, j, k) = a(i, j, k) * 2;
    A0.set_bounds(i,0,SIZE,
                  j,0,SIZE,
                  k,0,SIZE);
    Target target2 = get_host_target();
    Buffer<int> golden = A0.realize(SIZE, target2);

    // Check correctness.
    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
}
