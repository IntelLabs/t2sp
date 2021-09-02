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
// A positive test for vectorize: vectorize producer and/or consumer.

#include "util.h"

#define OI   4
#define II   4
#define SIZE II * OI

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 1, "a");
    Func A(PLACE0), B(PLACE0), A_feeder(PLACE1);
    Var oi, ii;
    B(ii, oi) = a(ii + II * oi);
    A(ii, oi) = B(ii, oi);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);

    // Generate input and run.
    Buffer<int> in = new_data<int, SIZE>(SEQUENTIAL); //or RANDOM
    a.set(in);
    Buffer<int> golden = A.realize({II, OI}, target);

    // Vectorize. compile.
    B.merge_ures(A).set_bounds(ii, 0, II, oi, 0, OI)
     .isolate_producer_chain(a, A_feeder);
#ifdef VEC_CONSUMER
    B.vectorize(ii);
#endif
#ifdef VEC_PRODUCER
    A_feeder.vectorize(ii);
#endif
    remove(getenv("BITSTREAM"));
    Buffer<int> out = A.realize({II, OI}, target);

    // Check correctness.
    check_equal<int>(golden, out);
    cout << "Success!\n";
}


