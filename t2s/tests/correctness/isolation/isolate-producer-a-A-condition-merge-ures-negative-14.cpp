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

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 1, "a");
    Func A(PLACE0), B(PLACE0), C(PLACE0);
    Var i;
    C(i) = (i <= 2);
    B(i) = (!C(i));
    A(i) = select(B(i) && (i < 5), a(i) + 1, i * 2) * 2;

    // Isolate. Re-compile.
    Func A_feeder(PLACE1);
    C.merge_ures(B, A);
    try {
        // B is inside the condition of a Select; the isolation below
        // should cause a compiler error to be thrown.
       C.isolate_producer_chain(B, A_feeder);
    } catch (...) {
        cout << "Success!\n";
    }
}


