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
#include <vector>
#define TYPE int
#define SIZE 16
#define N 4
#define TASKNUM 99

int main(void) {
    // setenv("WAIT", "wait", 1);
    // Declare 2 IPs on FPGA
    Func A0(Place::Device), A1(Place::Device);
    // Declare IPs' inputs
    ImageParam A(Int(32), 1);
    // Definitions of IPs
    Var iter;
    A0(iter) = A(iter) + 1;
    A1(iter) = A(iter) + 1;

    // Definitions of programming interface
    A0.command(0, INPUT_ONLY(), OUTPUT_ONLY(), INOUT(A));
    A1.command(1, INPUT_ONLY(), OUTPUT_ONLY(), INOUT(A));

    // Create overlay and compile
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    auto overlay = Overlay(A0, A1).compile(target, "overlay");

    // Declare many tasks
    Func Task_begin(Place::Device);
    std::vector<Func> Tasks;
    for (int k = 0; k < TASKNUM; k++) {
        Tasks.push_back(Func(Place::Device));
    }
    ImageParam a(Int(32), 1);
    Buffer<TYPE> inout = new_data<TYPE, SIZE>(SEQUENTIAL);
    Buffer<TYPE> result = new_data<TYPE, SIZE>(RANDOM);
    for (int i = 0; i < SIZE; i++) {
        result(i) = inout(i) + (TASKNUM + 1) * N;
    }
    a.set(inout);

    Var i;
    Task_begin(i) = enqueue(overlay, 1, a);
    for (int k = 0; k < TASKNUM; k++) {
        Tasks[k](i) = enqueue(overlay, k % 2, a);
    }
    Task_begin.merge_ures(Tasks).set_bounds(i, 0, N);

    // Dependency definition
    Task_begin.depend(Tasks[TASKNUM - 1], i - 1, i > 0);
    Tasks[0].depend(Task_begin, i);
    for (int k = 1; k < TASKNUM; k++) {
        Tasks[k].depend(Tasks[k - 1], i);
    }

    inout.set_host_dirty();
    Tasks[TASKNUM - 1].realize(target);
    inout.set_device_dirty();
    inout.copy_to_host();
    // for (int j = 0; j < SIZE; j++) {
    //     printf("%d ", inout(j));
    // }
    // printf("\n");
    check_equal<TYPE>(result, inout);
    cout << "Success!\n";
}
