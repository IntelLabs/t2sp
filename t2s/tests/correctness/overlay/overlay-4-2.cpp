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
#define N 4

int main(void) {
    // setenv("WAIT", "wait", 1);
    // Declare 2 IPs on FPGA
    ImageParam A(Int(32), 1);
    Expr e = Variable::make(Int(32), "e");
    Func A0(Place::Device);

    // Definitions of IPs
    Var iter;

    A0(iter) = A(iter) + e;
    // Definitions of programming interface
    A0.command(0, INPUT_ONLY(e), OUTPUT_ONLY(), INOUT(A));

    // Create overlay and compile
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    auto overlay = Overlay(A0).compile(target, "overlay");

    Func Task0(Place::Device);
    ImageParam a(Int(32), 1);
    Buffer<TYPE> inout = new_data<TYPE, SIZE>(SEQUENTIAL);
    Buffer<TYPE> result(SIZE);
    for (int i = 0; i < SIZE; i++) {
        result(i) = inout(i) + N;
    }
    a.set(inout);

    // Create tasks with actual inputs (buffers and values)
    Var i;
    Task0(i) = enqueue(overlay, 0, Expr(1), a);

    inout.set_host_dirty();
    Task0.realize({N}, target);
    inout.set_device_dirty();
    inout.copy_to_host();

    check_equal<TYPE>(result, inout);
    // for (int j = 0; j < SIZE; j++) {
    //     printf("%d ", inout(j));
    // }
    // printf("\n");
    cout << "Success!\n";
    // unsetenv("WAIT");
}
