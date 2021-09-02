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
#define N 8

int main(void) {
    // setenv("WAIT", "wait", 1);
    ImageParam A(Int(32), 2, "A");

    Func A0(Place::Device);

    Var x, y;
    A0(x, y) = A(x, y) + 1;

    A0.command(0, INPUT_ONLY(), OUTPUT_ONLY(), INOUT(A));

    // Create overlay and compile
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);
    auto overlay = Overlay(A0).compile(target, "overlay");

    Func Task0(Place::Device);

    ImageParam a(Int(32), 2);
    TYPE data[256] = {0};
    Buffer<TYPE> input(data, SIZE, SIZE);
    a.set(input);

#define m 2
#define a_ii a.BCropped(m, i, i, i, i)
    Var i;
    Task0(i) = enqueue(overlay, 0, a_ii);

    // Note: If the input is not set as dirty, it won't be copied to the device.
    input.set_host_dirty();
    Task0.realize({N}, target);
    input.set_device_dirty();
    input.copy_to_host();

    // for (int j = 0; j < SIZE; j++) {
    //     for (int i = 0; i < SIZE; i++) {
    //         printf("%d ", input(i, j));
    //     }
    //     printf("\n");
    // }
    // printf("\n");
    TYPE result[SIZE * SIZE] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1};
    Buffer<TYPE> ground(result, SIZE, SIZE);
    check_equal_2D<TYPE>(ground, input);
    cout << "Success!\n";
}
