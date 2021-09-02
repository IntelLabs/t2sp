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
// Run ./test.sh and see the commands to compile and run this test in the output logs (success.txt and failure.txt)

#include "util.h"

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 1, "a");
    Func A(Place::Device), B(Place::Host);;
    Var i;
    A(i) = a(i) * 2;
    B(i) = a(i) * 2;

    // Generate golden output.
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);
#ifdef REALIZE
    // Generate input and run.
    Buffer<int> in = new_data<int, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(in);
    Buffer<int> golden = B.realize(SIZE, target);
#endif

    // Isolate out a producer and consumer on the host. Recompile and run.
    Func Loader(PLACE1), Drainer(PLACE2);
    A.isolate_producer(a, Loader)
     .isolate_consumer(Drainer);

#ifdef COMPILE
    Drainer.compile_jit(target);
#endif

#ifdef REALIZE
    Buffer<int> out = Drainer.realize(SIZE, target);
    // Check correctness.
    check_equal<int>(golden, out);
#endif

    cout << "Success!\n";
}
