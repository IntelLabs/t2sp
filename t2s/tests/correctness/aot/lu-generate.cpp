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
// The only header file needed for including T2S.
#include "Halide.h"

// For printing output
#include <stdio.h>

// For validation of results.
#include <assert.h>

using namespace Halide;
using namespace std;

// Input matrices: A(SIZE, SIZE)
#define SIZE 8

// Input matrix a is 2-dimensional matrices of TYPE (float32).
#define TYPE Float(32)
ImageParam A(TYPE, 2);

// Implementation of the compute.
Func lu_decomposition() {
    // Macros for the convenience of writing UREs.
    // Iterations:
    #define X                      i,     j,     k
    #define X_no_k                 i,     j
    #define X_k_minus_1            i,     j,     k - 1
    #define X_j_minus_1            i,     j - 1, k
    #define X_i_minus_1            i - 1, j,     k

    // Loop variables
    Var  X;

    // UREs. All are recursive functions, and need signatures to be declared. An exception is O, the function
    // for the final results, which is not really a recursive Func, and declaring its place is enough.
    Func PrevV("PrevV", TYPE, {X}, Place::Device), 
         V("V", TYPE, {X}, Place::Device), 
         L("L", TYPE, {X}, Place::Device), 
         U("U", TYPE, {X}, Place::Device), 
         Z("Z", TYPE, {X}, Place::Device),
         O("O", Place::Device);
    PrevV(X)  = select(i >= k, select(k == 0, A(i, j), V(X_k_minus_1)), 0);
    U(X)      = select(i >= k, select(j == k, PrevV(X), U(X_j_minus_1)), 0);
    L(X)      = select(j == k || i < k, 0 /*Arbitrary value, as it is undefined in this case.*/,
                               select(i == k, PrevV(X) / U(X_j_minus_1), L(X_i_minus_1)));
    V(X)      = select(j == k || i < k, 0 /*Arbitrary value, as it is undefined in this case.*/,
                               PrevV(X) - L(X) * U(X_j_minus_1));
    Z(X)      = select(i >= k, select(j == k, U(X), select(i == k, L(X), 0)), select(k > 0, Z(X_k_minus_1), 0));

    O(X_no_k) = select(j == k, Z(X));

    PrevV.merge_ures(U, L, V, Z, O) // Put all the UREs inside the same loop nest. Now the first URE (PrevV) represents all the UREs.
         .reorder(j, k, i)
         .set_bounds(k, 0, SIZE, j, k, SIZE - k, i, 0, SIZE)    // Explicitly set the loop bounds
         .space_time_transform(j, k);

    // Isolate out the I/O network.
    // The arguments of the new functions will be generatd automatically. One need set the places.
    Func serializer("serializer", Place::Host), loader("loader", Place::Device), feeder("feeder", Place::Device), 
         collector("collector", Place::Device), unloader("unloader", Place::Device), deserializer("deserializer", Place::Host);

    PrevV.isolate_producer_chain(A, serializer, loader, feeder);
    serializer.set_bounds(k, 0, 1, j, 0, SIZE);
    loader.set_bounds(k, 0, 1, j, 0, SIZE);
    feeder.set_bounds(k, 0, 1, j, 0, SIZE);
    feeder.scatter(loader, j);
    loader.min_depth(512);
    feeder.min_depth(512);

    O.isolate_consumer_chain(collector);
    collector.space_time_transform(j)
	        .set_bounds(i, 0, SIZE)
             .set_bounds(j, 0, SIZE);
    collector.isolate_consumer_chain(unloader);
    collector.gather(O, j);
    unloader.isolate_consumer_chain(deserializer);
    O.min_depth(512);
    collector.min_depth(512);
    unloader.min_depth(512);

    // Return the (unique) output function The compiler will be able to find all the other functions from it.
    return deserializer;
}

int main() {
    Target target = get_host_target();     // Get the CPU host
    target.set_feature(Target::IntelFPGA); // To execute on an Intel FPGA device attached to the host.
    target.set_feature(Target::EnableSynthesis);
    Func lu = lu_decomposition();           // Get the compute.

    std::vector<Argument> args = {A};
    // generate host file and synthesis the design into bitstream
    lu.compile_to_host("host", args, "LU", target);
}



