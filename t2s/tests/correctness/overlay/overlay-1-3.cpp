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
#define SIZE 2
#define N 4

int main(void) {
    // setenv("WAIT", "wait", 1);
    // Declare 2 IPs on FPGA
    Func A0(Place::Device), A1(Place::Device);
    // Declare IPs' inputs
    ImageParam A(Int(32), 3);
    // Definitions of IPs
    Var x, y, z;
    A0(x, y, z) = A(x, y, z) + 1;
    A1(x, y, z) = A(x, y, z) + 1;

    // Definitions of programming interface
    A0.command(0, INPUT_ONLY(), OUTPUT_ONLY(), INOUT(A));
    A1.command(1, INPUT_ONLY(), OUTPUT_ONLY(), INOUT(A));

    // Create overlay and compile
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    auto overlay = Overlay(A0, A1).compile(target, "overlay");

    Func Task0(Place::Device), Task1(Place::Device);
    ImageParam a(Int(32), 3);
    TYPE data[] = {0, 1, 2, 3, 4, 5, 6, 7};
    Buffer<TYPE> inout(data, SIZE, SIZE, SIZE);
    Buffer<TYPE> result(SIZE, SIZE, SIZE);
    for (int k = 0; k < SIZE; k++) {
        for (int j = 0; j < SIZE; j++) {
            for (int i = 0; i < SIZE; i++) {
                result(i, j, k) = inout(i, j, k) + 2 * N;
            }
        }
    }
    a.set(inout);
    // Create tasks with actual inputs (buffers and values)
    Var i;
    Task0(i) = enqueue(overlay, 0, a);
    Task1(i) = enqueue(overlay, 1, a);
    Task0.merge_ures(Task1).set_bounds(i, 0, N);
    // Dependency definition
    Task0.depend(Task1, i - 1, i > 0);
    Task1.depend(Task0, i);

    inout.set_host_dirty();
    Task1.realize(target);
    inout.set_device_dirty();
    inout.copy_to_host();

    check_equal_3D<TYPE>(result, inout);
    cout << "Success!\n";
    // unsetenv("WAIT");
}
