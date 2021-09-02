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
// A negative test for vectorize: cannot vectorize a Func already merged.

#include "util.h"

#define II 16

int main(void) {
    // Define the compute.
    ImageParam b(Int(32), 1, "b");
    Func A(PLACE0), B(PLACE0);
    Var i, oi, ii;
    B(i) = b(i);
    A(i) = B(i);
    B.compute_root();
    A.split(i, oi, ii, II);
    B.split(i, oi, ii, II);

    // Vectorize. compile.
    A.merge_ures(B);
    A.vectorize(ii);
    try {
        B.vectorize(ii);
    } catch (Halide::CompileError & e) {
        cout << e.what() << "Success!\n";
        exit(0);
    }
    cout << "Failed!\n";
    exit(1);
}


