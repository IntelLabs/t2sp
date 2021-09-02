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
// To debug the correctness of the UREs (on the host):
//    g++ LU-basic.cpp -O0 -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin -lHalide -lpthread -ldl -std=c++11 -DDEBUG_URES -DSIZE=1(or 2, 3,..., 6)
//    ./a.out
//
// To compile and run on the FPGA emulator:
//    g++ LU-basic.cpp -O0 -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin -lHalide -lpthread -ldl -std=c++11 -DSIZE=1(or 2, 3,..., 6)
//    rm ~/tmp/a.* ~/tmp/a -rf
//    env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 AOC_OPTION="-march=emulator -O0 -g " ./a.out

#include "util.h"

#define TYPE        float
#define HALIDE_TYPE Float(32)
#define BATCH_TILE 14
#define BATCH BATCH_TILE * 2


int main(void) {
    ImageParam A(type_of<TYPE>(), 2);

    // Macros: for convenient use.
    #define X                      i,     bb,    j,     k,     b
    #define X_no_k                 i,     bb,    j,     b
    #define X_k_minus_1            i,     bb,    j,     k - 1, b
    #define X_j_minus_1            i,     bb,    j - 1, k,     b
    #define X_i_minus_1            i - 1, bb,    j,     k,     b
    #define FUNC_DECL              HALIDE_TYPE, {X}, PLACE1

    Var  X;
    Func PrevV(FUNC_DECL), V(FUNC_DECL), L(FUNC_DECL), PreU(FUNC_DECL), U(FUNC_DECL), Z(FUNC_DECL), // A recurrent Func needs declare return type, args, and place.
         O(PLACE1);         // A non-recurrent Func needs declare only its place.

    PrevV(X)  = select(i >= k && j >= k, select(k == 0, A(i, j), V(X_k_minus_1)), 0);
    PreU(X)   = select(i >= k && j > k, U(X_j_minus_1), 0);
    U(X)      = select(i >= k && j >= k, select(j == k, PrevV(X), PreU(X)), 0);
    L(X)      = select(i < k || j <= k, 0 /*Arbitrary value, as it is undefined in this case.*/,
                               select(i == k, PrevV(X) / PreU(X), L(X_i_minus_1)));
    V(X)      = select(i < k || j <= k, 0 /*Arbitrary value, as it is undefined in this case.*/,
                               PrevV(X) - L(X) * PreU(X));
    Z(X)      = select(j >= k, select(i < k, Z(X_k_minus_1), select(j == k, U(X), select(i == k, L(X), 0))), 0);
    O(X_no_k) = select(j == k, Z(X));
    
    PrevV.merge_ures(PreU, U, L, V, Z, O) // Put all the UREs into the same loop nest
         .reorder(j, bb, k, i, b)
         .set_bounds(k, 0, SIZE, j, 0, SIZE, i, 0, SIZE)
         .set_bounds(bb, 0, BATCH_TILE, b, 0, BATCH / BATCH_TILE)
         .space_time_transform(j);

    Func serializer(PLACE0), feeder(PLACE1), loader(PLACE1);
    PrevV.isolate_producer_chain(A, serializer, loader, feeder);
    feeder.scatter(loader, j);
    feeder.min_depth(256);
    loader.min_depth(256);
    //feeder.buffer(loader, j, BufferStrategy::Double);

    O.min_depth(256);
    Func deserializer(PLACE0), collector(PLACE1), unloader(PLACE1);
    O.isolate_consumer_chain(collector);
    collector.space_time_transform(j)
	     	 .set_bounds(i, 0, SIZE)
             .set_bounds(j, 0, SIZE)
             .set_bounds(bb, 0, BATCH_TILE, b, 0, BATCH / BATCH_TILE);
    collector.isolate_consumer_chain(unloader);
    collector.gather(O, j);
    unloader.isolate_consumer_chain(deserializer);
    collector.min_depth(256);
    unloader.min_depth(256);
    // Generate input and run.
    //A.dim(0).set_bounds(0, SIZE).set_stride(1);
    //A.dim(1).set_bounds(0, SIZE).set_stride(SIZE);
    //unloader.output_buffer().dim(0).set_bounds(0, SIZE).set_stride(1);
    //unloader.output_buffer().dim(1).set_bounds(0, SIZE).set_stride(SIZE);

#if SIZE == 1
    TYPE data[] = {2};  // Expect output: 2
    Buffer<TYPE> input(data, 1, 1);
#elif SIZE == 2
    TYPE data[] = { 1, 3, 4, 5}; // Expect output: 1 3; 4 -7
    Buffer<TYPE> input(data, 2, 2);
#elif SIZE == 3
    TYPE data[] = { 1, 3, 9, 4, 6, 10, 2, 5, 3}; // Expect output: 1 3 9; 4 -6 -26; 2 0.17 -10.67
    Buffer<TYPE> input(data, 3, 3);
#elif SIZE == 4
    TYPE data[] = { 1, 4, 5, 2, 2, 3, 6, 7, 3, 5, 8, 9, 5, 2, 1, 12}; // Expect output: 1 4 5 2; 2 -5 -4 3; 3 1.4 -1.4 -1.2; 5 3.6 6.86 -0.57
    Buffer<TYPE> input(data, 4, 4);
#elif SIZE == 5
    TYPE data[] = { 2, 4, 5, 2, 5, 1, 4, 6, 7, 6, 3, 5, 5, 9, 7, 5, 2, 1, 6, 8, 5, 6, 7, 8, 7}; // Expect output: 2 4 5 2 5; 0.5 2 3.5 6 3.5; 1.5 -0.5 -0.75 9 1.25; 2.5 -4 -3.33 55 13.67; 2.5 -2 -2 0.6 -4.2
    Buffer<TYPE> input(data, 5, 5);
#elif SIZE == 6
    TYPE data[] = { 3, 1, 2, 5, 4, 6, 1, 2, 3, 7, 6, 8, 3, 4, 5, 9, 7, 7, 5, 10, 1, 2, 8, 8, 5, 6, 5, 5, 10, 9, 6, 3, 7, 9, 11, 12};
    Buffer<TYPE> input(data, 6, 6);
#else
    assert(false);
#endif
    // Note: If the input is not set as dirty, it won't be copied to the device.
    input.set_host_dirty();

    A.set(input);
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);
    Buffer<TYPE> results = deserializer.realize({SIZE, BATCH_TILE, SIZE, BATCH / BATCH_TILE}, target);

    cout << "Success!\n";
    return 0;
}

