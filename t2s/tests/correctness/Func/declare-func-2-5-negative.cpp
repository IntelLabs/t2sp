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
// A negative test case: Declared by Int(32, 3), {i, j}

#include "util.h"

int main(void) {
    // Define the compute.
    Var i, j;
    // Front-end code shouldn't use vector types. Instead vectorize a function.
    // So, I don't know make it valid and this feature is irrelevant for now.
    // So, let it be negative case.
    Func f(Int(32, 3), {i, j}), g;

    try {
        g(i, j) = f(i, j - 1);
        f(i, j) = i + j;
    } catch (Halide::InternalError & e) {
        cout << e.what() << "Success!\n";
        exit(0);
    }
    
    cout << "Failed!\n";
    exit(1);
}
