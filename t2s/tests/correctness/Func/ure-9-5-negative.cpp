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
// A negative test case: Self reference without initial value

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j;
    Func f(Int(32), {i, j}), g(Int(32), {i, j}), h;

    f(i, j) = 0;
    g(i, j) = f(i - 10, j) + g(i - 1, j);
    h(i, j) = g(i - 1, j) + f(i - 1, j);
    f.merge_ures(g, h);

    try {
        h.compile_jit();
    } catch (Halide::CompileError & e) {
        cout << e.what() << "Success!\n";
        exit(0);
    }
    cout << "Failed!\n";
    exit(1);
}
