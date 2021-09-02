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
#include "Halide.h"

// For printing output
#include <stdio.h>

// For validation of results.
#include <assert.h>

using namespace Halide;
using namespace std;

// Input matrices: A(K, I)  and B(J, K). Following Halide's convention, they are in column-major format.
#define I    (a.dim(1).extent() / (III * II))
#define J    (b.dim(0).extent() / (JJJ * JJ))
#define K    (a.dim(0).extent() / (KKK * KK))
#define II   4
#define JJ   4
#define KK   256
#define III  2
#define JJJ  4
#define KKK  4

// Input matrix a and b are 2-dimensional matrices of TYPE (float32).
#define TYPE Float(32)
ImageParam   a(TYPE, 2);
ImageParam   b(TYPE, 2);

// Implementation of the compute.
Func matrix_multiply() {
    // Macros for the convenience of writing UREs.
    // Iterations:
    #define P             kkk,           jjj,     iii,     jj, ii, kk,          k,     j, i
    #define P_iii_minus_1 kkk,           jjj,     iii - 1, jj, ii, kk,          k,     j, i // To be used only when iii != 0
    #define P_jjj_minus_1 kkk,           jjj - 1, iii,     jj, ii, kk,          k,     j, i // T0 be used only when jjj != 0
    #define P_kkk_minus_1 kkk - 1,       jjj,     iii,     jj, ii, kk,          k,     j, i // To be used only when kkk != 0
    #define P_kk_minus_1  kkk + KKK - 1, jjj,     iii,     jj, ii, kk - 1,      k,     j, i // To be used only when kkk == 0 and kk != 0
    #define P_k_minus_1   kkk + KKK - 1, jjj,     iii,     jj, ii, kk + KK - 1, k - 1, j, i // To be used only when kkk == 0, kk == 0 and k != 0
    #define P_c                          jjj,     iii,     jj, ii,                     j, i // Dimensions for the output
    // Linearized addresses:
    #define total_i       (iii + III * ii + III * II * i)
    #define total_j       (jjj + JJJ * jj + JJJ * JJ * j)
    #define total_k       (kkk + KKK * kk + KKK * KK * k)

    // Loop variables
    Var  kkk("kkk"), jjj("jjj"), iii("iii"), kk("kk"), jj("jj"), ii("ii"), k("k"), j("j"), i("i");

    // UREs. All are recursive functions, and need signatures to be declared. An exception is c, the function
    // for the final results, which is not really a recursive Func, and declaring its place is enough.
    Func A("A", TYPE, {P}, Place::Device), // Name (optional), return type, arguments and Place.
         B("B", TYPE, {P}, Place::Device),
         C("C", TYPE, {P}, Place::Device),
         c("c", Place::Device);
    A(P)   = select(jjj == 0, a(total_k, total_i), A(P_jjj_minus_1));
    B(P)   = select(iii == 0, b(total_j, total_k), B(P_iii_minus_1));
    C(P)   = select((kkk == 0) && kk == 0 && k == 0, 0,
                    select(kkk == 0, select(kk == 0, C(P_k_minus_1), C(P_kk_minus_1)), C(P_kkk_minus_1))
                   ) + A(P) * B(P);
    c(P_c) = select((kkk == KKK - 1) && (kk == KK -1) && (k == K - 1), C(P));

    // Put all the UREs inside the same loop nest. Now the first URE (A) represents all the UREs.
    A.merge_ures(B, C, c);

    // Explicitly set the loop bounds
    A.set_bounds(kkk, 0, KKK, jjj, 0, JJJ, iii, 0, III)
     .set_bounds(kk,  0, KK,  jj,  0, JJ,  ii,  0, II)
     .set_bounds(k,   0, K,   j,   0, J,   i,   0, I);

    A.space_time_transform(kkk, jjj, iii)
     .vectorize(kkk);

    // Isolate out the I/O network.
    // The arguments of the new functions will be generatd automatically. One need set the places.
    Func aSerializer("aSerializer", Place::Host), aLoader("aLoader", Place::Device),
         aFeeder("aFeeder", Place::Device), bSerializer("bSerializer", Place::Host),
         bLoader("bLoader", Place::Device), bFeeder("bFeeder", Place::Device),
         drainer("drainer", Place::Device), collector("collector", Place::Device),
         unloader("unloader", Place::Device), deserializer("deserializer", Place::Host);

    // Isolate the loading of matrix a into a pipeline: aSerializer --> aLoader --> aFeeder
    A.isolate_producer_chain(a, aSerializer, aLoader, aFeeder);

    // Isolate the loading of matrix b to another pipeline: bSerializer --> bLoader --> bFeeder
    A.isolate_producer_chain(b, bSerializer, bLoader, bFeeder);

    // Isolate the result c to an output pipeline c --> drainer --> collector --> unloader --> deserializer.
    // Here we first isolate drainer. It inherits the result c's args, P_c, which has less loops than the
    // systolic array (the reduction loops kkk, kk and k are gone). For this new loop structure, do
    // space-time transform with jjj and iii as the space loops, consistent with the systolic array.
    c.isolate_consumer(drainer);
    drainer.space_time_transform(jjj, iii);
    // Isolate the other functions in the output pipeline. They have the same loop structure as drainer.
    drainer.isolate_consumer_chain(collector, unloader, deserializer);

    // Optimize the I/O network.

    // The minimum number of registers on channels. Each channel is between two device functions. One may
    // need tune the number for each channel so that reading of the channel is not a performance bottlneck.
    #define CH_DEPTH    256
    #define c_CH_DEPTH  II * JJ // Each PE in the systolic array produces II * JJ elements in the current
                                // tile of the output matrix. Make the channel that deep so as a PE can
                                // drain all its results to the channel and continue work for the next tile.

    // On the host side, we can remove all j loops since matrix a has no dimension related with them.
    // Our runtime will send the resulting data, which are the serialized values of matrix a, into the device
    // memory. Because the resulting data will be located in memory, where the same data can be loaded as many
    // time aLoader wants, the removal of these j dimensions from aSerializer does not affect aLoader at all.
    aSerializer.remove(jjj, jj, j);

    // Load from the device memory the matrix a's values that are needed for computing 1 output tile only once.
    aLoader.remove(jjj, jj);
    // Insert some minimum number of registers on the output channel of aLoader to effectively decouple aLoader
    // from its consumer (aFeeder).
    aLoader.min_depth(CH_DEPTH);

    // Since aLoader sends less data by removing its jjj and jj loop, in aFeeder, a buffer has to be created, so
    // that the same data can be read from the buffer multiple times. The buffer must be inserted at a loop level
    // (e.g. ii or k) that encloses the two removed loops in aLoader.
    aFeeder.buffer(aLoader, k);
    // For better scalability, scatter the data vertically across the aFeeder PEs.
    aFeeder.scatter(aLoader, iii);
    // Insert some minimum number of registers on the output channel of aFeeder to effectively decouple aFeeder
    // from its consumer (the systolic array).
    aFeeder.min_depth(CH_DEPTH);

    // The input path for matrix b is optimized similarly to that for matrix a.
    bSerializer.remove(iii, ii, i);
    bLoader.remove(iii, ii).min_depth(CH_DEPTH);
    bFeeder.buffer(bLoader, k).scatter(bLoader, jjj).min_depth(CH_DEPTH);

    c.min_depth(c_CH_DEPTH);

    // Gather the output vertically across the drainer PEs.
    drainer.gather(c, iii).min_depth(CH_DEPTH);

    // Gather the output horizontally across the collector PEs into a vector.
    collector.gather(drainer, jjj).vectorize(jjj).min_depth(CH_DEPTH);

    // Save the output in vectors into device memory
    unloader.vectorize(jjj);

    // Our runtime will transfer the output in the device memory to the host memory. Now we
    // can deserialize the output data (i.e. save the sequential data to the correct multi-
    // dimensional address as dicated by P_c).
    deserializer.vectorize(jjj);

    // Return the (unique) output function The compiler will be able to find all the other functions from it.
    return deserializer;
}

int main() {
    Target target = get_host_target();     // Get the CPU host
    target.set_feature(Target::IntelFPGA); // To execute on an Intel FPGA device attached to the host.
    target.set_feature(Target::EnableSynthesis);
    Func mm = matrix_multiply();           // Get the compute.

    std::vector<Argument> args = {a, b};
    // generate host file and synthesis the design into bitstream
    mm.compile_to_host("host", args, "GEMM", target);
}



