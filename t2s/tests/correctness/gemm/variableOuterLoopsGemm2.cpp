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

/* We want to implement the following compute:

   // The outermost dimensions enumerate tiles of the output matrix c. The extents I, J, K
   // are determined by the actual sizes of the input matrices, which can be unknown to the compiler.
   for i in [0, I)
     for j in [0, J)
       for k in [0, K)
         // Compute one tile of the output matrix c. This is to be implemented by a systolic array.
         // All extents (i.e. II, JJ, KK, III, JJJ, and KKK) are static constants.
         for ii in [0, II)
           for jj in [0, JJ)
             for kk in [0, KK)
               for iii in [0, III)
                 for jjj in [0, JJJ)
                   for kkk in [0, KKK)
                     i' = iii + III * ii + III * II * i
                     j' = jjj + JJJ * jj + JJJ * JJ * j
                     k' = kkk + KKK * kk + KKK * KK * k
                     c(jjj, iii, jj, ii, j, i) += a(k', i') * b(j', k')
*/

#define TINY

#ifdef TINY
    #define II   3 
    #define JJ   3
    #define KK   5
    #define III  3
    #define JJJ  5
    #define KKK  2
#endif
#ifdef MEDIUM
    #define II   8
    #define JJ   8
    #define KK   8
    #define III  5
    #define JJJ  8
    #define KKK  4
#endif
#ifdef LARGE
    #define II   32
    #define JJ   32
    #define KK   32
    #define III  11
    #define JJJ  8
    #define KKK  16
#endif

// Input parameters: a and b are 2D float32 matrices.
ImageParam a(Float(32), 2);
ImageParam b(Float(32), 2);

// Implementation of the compute.
Func SGEMM() {
    // Macros for the convenience of writing UREs.
    #define P             kkk,           jjj,     iii,     jj, ii, kk,          k,     j, i
    #define P_iii_minus_1 kkk,           jjj,     iii - 1, jj, ii, kk,          k,     j, i // To be used only when iii != 0
    #define P_jjj_minus_1 kkk,           jjj - 1, iii,     jj, ii, kk,          k,     j, i // T0 be used only when jjj != 0
    #define P_kkk_minus_1 kkk - 1,       jjj,     iii,     jj, ii, kk,          k,     j, i // To be used only when kkk != 0
    #define P_kk_minus_1  kkk + KKK - 1, jjj,     iii,     jj, ii, kk - 1,      k,     j, i // To be used only when kkk == 0
    #define P_k_minus_1   kkk + KKK - 1, jjj,     iii,     jj, ii, kk + KK - 1, k - 1, j, i // To be used only when kkk == 0 and kk == 0
    #define P_c                          jjj,     iii,     jj, ii,                     j, i // Dimensions for the output
    #define total_i       (iii + III * ii + III * II * i)
    #define total_j       (jjj + JJJ * jj + JJJ * JJ * j)
    #define total_k       (kkk + KKK * kk + KKK * KK * k)
    #define ARGS_DEVICE   {P}, Place::Device
    #define I             (a.dim(1).extent() / (III * II))
    #define J             (b.dim(0).extent() / (JJJ * JJ))
    #define K             (a.dim(0).extent() / (KKK * KK))

    // Step 1 (Optional): Check the layout of the input data to be safe
    // -----------------------------------------------------------------------------------------------
    _halide_user_assert(evaluate<bool>(a.dim(0).extent() == b.dim(1).extent())) << "Matrix a and b's k dimension should be equal\n";
    _halide_user_assert(evaluate<bool>(a.dim(0).extent() == (KKK * KK * K))) << "Dimension k should be divisible by " << KKK * KK << "\n";
    _halide_user_assert(evaluate<bool>(a.dim(1).extent() == (III * II * I))) << "Dimension i should be divisible by " << III * II << "\n";
    _halide_user_assert(evaluate<bool>(b.dim(0).extent() == (JJJ * JJ * J))) << "Dimension j should be divisible by " << JJJ * JJ << "\n";

    // Tell the compiler the layout of the input data so that the compiler might use them to optimize the systolic array.
    // The input matrices are in column-major order. But it is possible that one can use the way below to realize
    // arbitary storage format, although we have not tested.
    a.dim(0).set_bounds(0, KKK * KK * K).set_stride(1);
    a.dim(1).set_bounds(0, III * II * I).set_stride(KKK * KK * K);
    b.dim(0).set_bounds(0, JJJ * JJ * J).set_stride(1);
    b.dim(1).set_bounds(0, KKK * KK * K).set_stride(JJJ * JJ * J);

    // Step 2: define UREs. All are recursive functions (except output c) and must declare their arguments before their definitions.
    // -----------------------------------------------------------------------------------------------

    Var  P;
    Func A(Float(32), ARGS_DEVICE), B(Float(32), ARGS_DEVICE), C(Float(32), ARGS_DEVICE), c(Place::Device),     // compute UREs
         ReductionStart(Bool(1), ARGS_DEVICE), ReductionEnd(Bool(1), ARGS_DEVICE), kkEQ0(Bool(1), ARGS_DEVICE); // control UREs

    // The following sub-computes contains kk and k. We would like to isolate them out of the systolic array
    // so that the array does not need to compute them, making the array simpler. We do not isolate sub-computes
    // related with kkk, because kkk loop will be kept inside the systolic array and vectorized for efficiency.
    Expr kk_k_eq_0   = (kk == 0      && k == 0);
    Expr kk_k_eq_max = (kk == KK - 1 && k == K - 1);
    Expr kk_eq_0     = (kk + 1 == 1); // A sub-compute to isolate must not be part of another sub-compute to isolate.
                                      // As kk == 0 is part of kk_k_eq_0, we change it to kk + 1 == 1 to workaround.

    // Control UREs. Pass the control signals horizontally through the systolic array.
    ReductionStart(P) = select(jjj == 0, kkk == 0       && kk_k_eq_0,   ReductionStart(P_jjj_minus_1));
    ReductionEnd(P)   = select(jjj == 0, kkk == KKK - 1 && kk_k_eq_max,   ReductionEnd(P_jjj_minus_1));
    kkEQ0(P)          = select(jjj == 0,                   kk_eq_0,              kkEQ0(P_jjj_minus_1));

    // Compute UREs. Values of matrix a/b are passed through the systolic array horizontally/vertically.
    A(P)   = select(jjj == 0, a(total_k, total_i), A(P_jjj_minus_1));
    B(P)   = select(iii == 0, b(total_j, total_k), B(P_iii_minus_1));
    C(P)   = select(ReductionStart(P), 0,
                    select(kkk == 0, select(kkEQ0(P), C(P_k_minus_1), C(P_kk_minus_1)),
                           C(P_kkk_minus_1))
                   ) + A(P) * B(P);
    c(P_c) = select(ReductionEnd(P), C(P));

    // Step 3 (Optional): Build a systolic array.
    // -----------------------------------------------------------------------------------------------

    // Put all the UREs inside the same loop nest. Now the first URE (ReductionStart) represents all the UREs.
    ReductionStart.merge_ures(ReductionEnd, kkEQ0, A, B, C, c);

    // Explicitly set the loop bounds
    ReductionStart.set_bounds(kkk, 0, KKK, jjj, 0, JJJ, iii, 0, III)
                  .set_bounds(kk,  0, KK,  jj,  0, JJ,  ii,  0, II)
                  .set_bounds(k,   0, K,   j,   0, J,   i,   0, I);

    // Space-time transform the loop nest. In this design, we make the innermost 3 loops as space loops,
    // and the outer loops will be treated as time loops by the compiler automatically.
    ReductionStart.space_time_transform(kkk, jjj, iii);

    // Vectorize the innermost loop
    ReductionStart.vectorize(kkk);

    // Step 3 (Optional): Isolate out the I/O network.
    // -----------------------------------------------------------------------------------------------

    // The arguments of the new functions will be generatd automatically. One need set the places.
    Func aSerializer(Place::Host), aLoader(Place::Device), aFeeder(Place::Device),
         bSerializer(Place::Host), bLoader(Place::Device), bFeeder(Place::Device),
         drainer(Place::Device), collector(Place::Device), unloader(Place::Device), deserializer(Place::Host);

    // Isolate the loading of matrix a, and all the control signals, into a pipeline: aLoader --> aFeeder
    ReductionStart.isolate_producer_chain({a, kk_k_eq_0, kk_k_eq_max, kk_eq_0}, aLoader, aFeeder);

    // Further isolate the loading of matrix a to another pipeline: aSerializer --> aLoader.
    aLoader.isolate_producer(a, aSerializer);

    // Isolate the loading of matrix b to another pipeline: bSerializer --> bLoader --> bFeeder
    ReductionStart.isolate_producer_chain(b, bSerializer, bLoader, bFeeder);

    // Isolate the result c to an output pipeline c --> drainer --> collector --> unloader --> deserializer.
    // Here we first isolate drainer. It inherits the result c's args, P_c, which has less loops than the
    // systolic array (the reduction loops kkk, kk and k are gone). For this new loop structure, do
    // space-time transform with jjj and iii as the space loops, consistent with the systolic array.
    c.isolate_consumer(drainer);
    drainer.space_time_transform(jjj, iii);
    // Isolate the other functions in the output pipeline. They have the same loop structure as drainer.
    drainer.isolate_consumer_chain(collector, unloader, deserializer);

    /* Step 4 (Optional): Optimize the I/O network. */
    // -----------------------------------------------------------------------------------------------

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

    // The system is composed of input paths --> systolic array --> output paths. We require all the output paths
    // converge at a unique last function. The compiler will be able to find all the other functions from this
    // last function. So return this unique last function to represent the system.
    return deserializer;
}

int main() {
    // Step 1: Set input. For example,
    const int OUTERMOST_I = 2;//32;
    const int OUTERMOST_J = 2;///32;
    const int OUTERMOST_K = 2;
    const int TOTAL_I = III * II * OUTERMOST_I;
    const int TOTAL_J = JJJ * JJ * OUTERMOST_J;
    const int TOTAL_K = KKK * KK * OUTERMOST_K;
    Buffer<float> ina = new_data_2d<float, TOTAL_K, TOTAL_I>(SEQUENTIAL); //or RANDOM
    Buffer<float> inb = new_data_2d<float, TOTAL_J, TOTAL_K>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);

    // Step 2: Run

    // Get the CPU host
    Target target = get_host_target();
    // To execute on an Intel FPGA device attached to the host. Dump out runtime info during execution
    target.set_features({Target::IntelFPGA, Target::Debug});
    // Get the systolic system.
    Func sgemm = SGEMM();
    // Invoke the T2S compiler to compile the systolic system and offload it to the FPGA to run.
    // The first argument below is the dimensions of the output matrix. After the compilation,
    // there will be a bitstream named as "a.aocx" under $HOME/tmp. As long as that bitstream is
    // there, even if one repeats the following command again, the compiler won't re-compile. You
    // can take advantage of that feature to test multiple times with various input matrices.
    Buffer<float> result = sgemm.realize({JJJ, III, JJ, II, OUTERMOST_J, OUTERMOST_I}, target);

    // Step 3: Validate the results
    for (size_t i = 0; i < OUTERMOST_I; i++) {
        for (size_t j = 0; j < OUTERMOST_J; j++) {
            for (size_t ii = 0; ii < II; ii++) {
                for (size_t jj = 0; jj < JJ; jj++) {
                    for (size_t iii = 0; iii < III; iii++) {
                        for (size_t jjj = 0; jjj < JJJ; jjj++) {
                            size_t i1 = iii + III * ii + III * II * i;
                            size_t j1 = jjj + JJJ * jj + JJJ * JJ * j;
                            float golden = 0.0f;
                            for (size_t k1 = 0; k1 < TOTAL_K; k1++) {
                                golden += ina(k1, i1) * inb(j1, k1);
                            }
                            //cout << "(" << j1 << ", " << i1 << ") = " << golden << " " << result(jjj, iii, jj, ii, j, i) << endl;
                            assert(fabs(golden - result(jjj, iii, jj, ii, j, i)) < 0.005*fabs(golden));
                        }
                    }
                }
            }
        }
    }
    cout << "Success!\n";
    return 0;
}



