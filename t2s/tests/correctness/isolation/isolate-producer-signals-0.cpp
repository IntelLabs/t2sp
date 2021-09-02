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

#define I 2
#define J 2

int main(void) {
    ImageParam a(Int(32), 2);
    Var j, i;
    Func Signal0(Bool(1), {j, i}, PLACE0), Signal1(Bool(1), {j, i}, PLACE0), A(Int(32), {j, i}, PLACE0);
    Expr signal0 = (i == 0);
    Expr signal1 = (i == I - 1);
    Signal0(j, i) = select(j == 0, signal0, Signal0(j - 1, i));
    Signal1(j, i) = select(j == 0, signal1, Signal1(j - 1, i));
    A      (j, i) = select(j == 0, a(j, i), select(Signal0(j, i), 1, select(Signal1(j, i), 2, A(j - 1, i))));

    Signal0.merge_ures(Signal1, A).set_bounds(j, 0, J, i, 0, I);

    // Compute a golden reference.
    Func B(Int(32), {j, i});
    B(j, i) = select(j == 0, a(j, i), select(i == 0, 1, select(i == I - 1, 2, B(j - 1, i))));
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    Buffer<int> ina = new_data_2d<int, J, I>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    Buffer<int> golden = B.realize({J, I}, target);

    // Isolate an input path
    Func P1(PLACE1), P2(PLACE2);
    Signal0.isolate_producer_chain({a, signal0, signal1}, P1, P2);

    Buffer<int> out = A.realize({J, I}, target);

    check_equal_2D<int>(golden, out);
    cout << "Success!\n";
}





