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

#define I 8
#define J 8
#define K 8
#define KI 4
#define KO K/KI

int main(void) {
    // Input parameters: a and b are 2D matrices.
    ImageParam a(type_of<int>(), 2);
    ImageParam b(type_of<int>(), 2);

    Var i, j, ko, ki;

    // Macros for convenience.
    #define P ki, i, j, ko
    #define k ki * 2 + ko

    #define control Int(32), {P}, PLACE1
    #define compute Int(32), {P}, PLACE1

    Func A(compute), B(compute), C(compute), c(PLACE1);             // Compute UREs
    A(P) = select(j == 0, a(i, k), A(ki, i, j-1, ko));
    B(P) = select(i == 0, b(k, j), B(ki, i-1, j, ko));
    C(P) = select(ki == 0, select(ko == 0, 0, C(ki+KI-1, i, j, ko-1)), C(ki-1, i, j, ko)) + A(P)*B(P);
    c(i, j) = select(k == (K-1), C(P));

    // Merge UREs
    A.merge_ures(B, C, c)
     .set_bounds(i, 0, I, j, 0, J)
     .set_bounds(ki, 0, KI, ko, 0, KO);
    A.space_time_transform({ki, i, j}, {1, 1, 1});
    A.vectorize(ki);

    // Generate input and run.
    Buffer<int> ina = new_data_2d<int, I, K>(SEQUENTIAL); //or RANDOM
    Buffer<int> inb = new_data_2d<int, K, J>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    Buffer<int> result = c.realize({I, J}, target);

    cout << "Success!\n";
    return 0;
}
    


