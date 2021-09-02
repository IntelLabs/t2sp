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
    Func A0(Place::Device), A1(Place::Device);
    // Declare IPs' inputs
    ImageParam A(Int(32), 1), B(Int(32), 1);
    // Definitions of IPs
    Var iter;
    A0(iter) = A(iter) + B(iter);
    A1(iter) = A(iter) + B(iter);

    // Definitions of programming interface
    A0.command(0, INPUT_ONLY(B), OUTPUT_ONLY(), INOUT(A));
    A1.command(1, INPUT_ONLY(B), OUTPUT_ONLY(), INOUT(A));

    // Create overlay and compile
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    auto overlay = Overlay(A0, A1).compile(target, "overlay");

    Func Task0(Place::Device), Task1(Place::Device), Task2(Place::Device);
    ImageParam a(Int(32), 1), b(Int(32), 1);
    Buffer<TYPE> inout1 = new_data<TYPE, SIZE>(SEQUENTIAL);
    Buffer<TYPE> inout2 = new_data<TYPE, SIZE>(SEQUENTIAL);
    Buffer<TYPE> result(SIZE);
    for (int i = 0; i < SIZE; i++) {
        result(i) = inout1(i) + 2 * N * inout2(i);
    }
    a.set(inout1);
    b.set(inout2);

    // Create tasks with actual inputs (buffers and values)
    Var i;
    Task0(i) = enqueue(overlay, 0, b, a);
    Task1(i) = enqueue(overlay, 1, b, a);
    Task0.merge_ures(Task1).set_bounds(i, 0, N);
    // Task1.compute_with(Task0, i);

    Task0.depend(Task1, i - 1, i > 0);
    Task1.depend(Task0, i);

    inout1.set_host_dirty();
    inout2.set_host_dirty();
    Task1.realize(target);
    inout1.set_device_dirty();
    inout1.copy_to_host();

    check_equal<TYPE>(result, inout1);
    // for (int j = 0; j < SIZE; j++) {
    //     printf("%d ", inout1(j));
    // }
    cout << "Success!\n";
    // unsetenv("WAIT");
}
