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
// A negative test case: two Funcs have the same command queue index.
#include "util.h"

#define TYPE int
#define SIZE 16
#define N 4

int main(void) {
    // Declare 2 IPs on FPGA
    Func A0(Place::Device), A1(Place::Device);
    // Declare IPs' inputs
    ImageParam A(Int(32), 1);
    // Definitions of IPs
    Var iter;
    A0(iter) = A(iter) + 1;
    A1(iter) = A(iter) + 1;

    try {
        // two Funcs have the same command queue index
        A0.command(0, {A} /* INPUTONLY */, {} /* OUTPUTONLY */, {} /* INOUT */);
        A1.command(0, {A} /* INPUTONLY */, {} /* OUTPUTONLY */, {} /* INOUT */);
    } catch (Halide::CompileError &e) {
        cout << e.what() << "Success!\n";
        exit(0);
    }
    cout << "Failed!\n";
    exit(1);
}
