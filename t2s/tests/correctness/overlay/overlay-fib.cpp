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

#define TYPE int
#define SIZE 16
#define N 1

int main(void) {
    Var iter;
    Func F(Int(32), {iter}, Place::Device);
    Expr f0 = Variable::make(Int(32), "f0"), f1 = Variable::make(Int(32), "f1");

    F(iter) = select(iter == 0, f0, select(iter == 1, f1, F(iter - 1) + F(iter - 2)));

    F.command(0, INPUT_ONLY(f0, f1), OUTPUT_ONLY(), INOUT());

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    auto overlay = Overlay(F).compile(target, "overlay");

    Func Task0(Place::Device);
    ImageParam a(Int(32), 1);
    Buffer<TYPE> output(SIZE);
    Buffer<TYPE> result(SIZE);
    result(0) = 1;
    result(1) = 1;
    for (int i = 2; i < SIZE; i++) {
        result(i) = result(i - 1) + result(i - 2);
    }
    a.set(output);

    // Create tasks with actual inputs (buffers and values)
    Var i;
    Task0(i) = enqueue(overlay, 0, Expr(1), Expr(1), a);

    Task0.realize({N}, target);
    output.set_device_dirty();
    output.copy_to_host();
    check_equal<TYPE>(result, output);
    cout << "Success!\n";
}
