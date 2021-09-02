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
    // TOFIX: this case currently has two issues
    // (1) If declared as
    //     Func A(PLACE0), out(PLACE0);
    //     Var i;
    //     Will encounter a compile error: value index out of range.
    //     This is with merge_ure().
    // (2) If declared as below, breads during lowring at Tracing.cpp Line 110 for Func A
    //     internal_assert(!f.can_be_inlined() || !f.schedule().compute_level().is_inlined());
    Var i;
    Func A(Int(32), {i}, PLACE0), out(PLACE0), B;
    A(i)   = select(i == 0, i + 1, A(i - 1));
    out(i) = A(i);
    B(i)   = 1;

    A.merge_ures(out)
     .set_bounds(i, 0, SIZE);
    
    // Compile and run
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    Buffer<int> golden = B.realize(SIZE, target);

    // Isolate. Re-compile and run.
    Func consumer(PLACE1);
    out.isolate_consumer(consumer);
    Buffer<int> result = consumer.realize(SIZE, target);

    // Check correctness.
    check_equal<int>(golden, result);
    cout << "Success!\n";
}
