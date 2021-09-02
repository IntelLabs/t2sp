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
    // Declare 2 IPs on FPGA
    Func A(Place::Device), B(Place::Device);
    // Declare IPs' inputs
    ImageParam X(Int(32), 1);
    Param<bool> read_input_from_buffer, store_output_to_buffer;
    // Definitions of IPs
    Var iter;
    A(iter) = select(read_input_from_buffer, B(iter - 1), X(iter));
    B(iter) = select(store_output_to_buffer, A(iter));
    A.merge_ures(B);

    // Definitions of programming interface
    B.command(0, {X, read_input_from_buffer, store_output_to_buffer}, {}, {});

    // Create overlay and compile
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    auto overlay = Overlay(B).compile(target, "overlay");

    Func Task0(Place::Device), Task1(Place::Device), Task2(Place::Device), Task3(Place::Device);
    Buffer<TYPE> input = new_data<TYPE, SIZE>(RANDOM);
    Buffer<TYPE> output = new_data<TYPE, SIZE>(RANDOM);
    Buffer<TYPE> dummy_in = new_data<TYPE, SIZE>(RANDOM);
    Buffer<TYPE> dummy_out = new_data<TYPE, SIZE>(RANDOM);

#define YES true
#define NO false
#define DUMMY_I dummy_in
#define DUMMY_O dummy_out
    // Create tasks with actual inputs (buffers and values)
    Var i;
    Task0(i) = enqueue(overlay, 0, input, NO, NO, output);
    Task1(i) = enqueue(overlay, 0, input, NO, YES, DUMMY_O);
    Task2(i) = enqueue(overlay, 0, DUMMY_I, YES, NO, output);
    Task3(i) = enqueue(overlay, 0, DUMMY_I, YES, YES, DUMMY_O);
    Task0.merge_ures(Task1, Task2, Task3);

    Task3.realize({N}, target);
    output.set_device_dirty();
    output.copy_to_host();

    void check(const Buffer<TYPE> &results, const Buffer<TYPE> &input);
    check(output, input);
    cout << "Success!\n";
}

void check(const Buffer<TYPE> &results, const Buffer<TYPE> &input) {
}
