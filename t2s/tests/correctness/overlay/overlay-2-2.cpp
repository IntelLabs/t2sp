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
#define SIZE 3
#define I 4
#define J 4

int main(void) {
    // setenv("WAIT", "wait", 1);
    // Declare 2 IPs on FPGA
    Func A0(Place::Device), A1(Place::Device);
    // Declare IPs' inputs
    ImageParam A(Int(32), 2);
    // Definitions of IPs
    Var x, y;

    A0(x, y) = A(x, y) + 1;
    A1(x, y) = A(x, y) + 1;

    // Definitions of programming interface
    A0.command(0, INPUT_ONLY(), OUTPUT_ONLY(), INOUT(A));
    A1.command(1, INPUT_ONLY(), OUTPUT_ONLY(), INOUT(A));

    // Create overlay and compile
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    auto overlay = Overlay(A0, A1).compile(target, "overlay");

    Func Task0(Place::Device), Task1(Place::Device), Task2(Place::Device);
    ImageParam a(Int(32), 2);
    TYPE data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    Buffer<TYPE> inout(data, SIZE, SIZE);
    Buffer<TYPE> result(SIZE, SIZE);
    for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < SIZE; i++) {
            result(i, j) = inout(i, j) + 3 * I * J;
        }
    }
    a.set(inout);
    // Create tasks with actual inputs (buffers and values)
    Var i, j;
    Task0(i, j) = enqueue(overlay, 0, a);
    Task1(i, j) = enqueue(overlay, 1, a);
    Task2(i, j) = enqueue(overlay, 1, a);
    Task0.merge_ures(Task1, Task2).set_bounds(i, 0, I, j, 0, J);
    // Dependency definition
    Task0.depend(Task1, i, j - 1, j > 0).depend(Task2, i - 1, j - 1, i > 0 && j > 0);
    Task1.depend(Task0, i, j).depend(Task2, i - 1, j, i > 0);
    Task2.depend(Task0, i, j).depend(Task1, i, j);
    // Task0.depend(Task2, i - 1, j - 1, i > 0 && j > 0);
    // Task1.depend(Task0, i, j);
    // Task2.depend(Task1, i, j);

    inout.set_host_dirty();
    Task2.realize(target);
    inout.set_device_dirty();
    inout.copy_to_host();

    check_equal_2D<TYPE>(result, inout);
    cout << "Success!\n";
}

void check(const Buffer<TYPE> &results, const Buffer<TYPE> &input) {
    bool pass = true;
    printf("*** Input:\n");
    for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < SIZE; i++) {
            printf("%d ", input(i, j));
        }
        printf("\n");
    }

    printf("*** Result:\n");
    for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < SIZE; i++) {
            printf("%d ", results(i, j));
            bool correct = (results(i, j) == input(i, j) + 1);
            pass = pass && correct;
        }
        printf("\n");
    }
    // assert(pass);
}
