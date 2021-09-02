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
    Var i;
    Func A(Int(32), {i}, PLACE0), B(PLACE0);
    B(i) = select(i == 0, 0, A(i-1));
    A(i) = B(i) * 2;

    // Isolate. Re-compile.
    Func A_feeder(PLACE1);
    B.merge_ures(A);
    try {
        // A and B are cyclically dependent. Isolating B requires to isolate A as well, which is wrong, since B(i) is used in defining A(i);
        // we want to isolate only that B(i) operand, not the entire definition (A(i)) that contains the operand. A compiler error should be thrown.
        B.isolate_producer_chain(B, A_feeder);
    } catch (...) {
        cout << "Success!\n";
    }
}


