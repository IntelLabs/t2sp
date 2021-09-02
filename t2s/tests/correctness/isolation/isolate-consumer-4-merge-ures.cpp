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
    // Compute golden reference results on the host.
    Var i;
    Func hA(Int(32), {i}), hout;
    hA(i)   = select(i == 0, i + 1, hA(i - 1));
    hout(i) = hA(i);

    hA.compute_root();

    Target target = get_host_target();
    Buffer<int> golden = hout.realize(SIZE, target);

    // Compute on the given places with merge_ures and isolation
    Func A(Int(32), {i}, PLACE0), out(PLACE0);
    A(i)   = select(i == 0, i + 1, A(i - 1));
    out(i) = A(i);

    A.merge_ures(out)
     .set_bounds(i, 0, SIZE);

    Func c1(PLACE1), c2(PLACE1), c3(PLACE1);
    out.isolate_consumer_chain(c1, c2, c3);
    target.set_features({Target::IntelFPGA, Target::Debug});
    Buffer<int> result = c3.realize(SIZE, target);

    // Check correctness.
    check_equal<int>(golden, result);
    cout << "Success!\n";
}
