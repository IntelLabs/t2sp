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
    Func A(PLACE0), B;
    Var i;
    A(i) = i * 2;
    B(i) = i * 2;

    // Compile and run
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    Buffer<int> golden = B.realize(SIZE, target);

    // Isolate. Re-compile and run.
    Func consumer(PLACE1);
    A.isolate_consumer(consumer);
    Buffer<int> result = consumer.realize(SIZE, target);

    // Check correctness.
    check_equal<int>(golden, result);
    cout << "Success!\n";
}
