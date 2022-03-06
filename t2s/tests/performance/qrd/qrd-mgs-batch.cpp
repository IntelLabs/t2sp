// g++ qrd-mgs-batch.cpp -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DVERBOSE_DEBUG
// env HL_DEBUG_CODEGEN=4 PRAGMAUNROLL=1 BITSTREAM="${HOME}/tmp/a.aocx" CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=${FPGA_BOARD} -emulator-channel-depth-model=strict " ./a.out

// The only header file needed for including T2S.
#include "Halide.h"

// For printing output
#include <stdio.h>

// For validation of results.
#include <assert.h>

using namespace Halide;
using namespace std;


#define TYPE     Float(32)
#define FuncType Func
#define ZERO     0
ImageParam a(Float(32), 3);

#define BATCH   a.dim(2).extent()
#define I       128
#define J       128
#define K       128

int main(void) {
    // Macros: for convenient use.
    #define P                      k,     j,     i,     b
    #define P_no_k                        j,     i,     b
    #define P_no_j                 k,            i,     b
    #define P_k_minus_1            k - 1, j,     i,     b
    #define P_j_minus_1            k,     j - 1, i,     b
    #define P_i_minus_1            k,     j,     i - 1, b

    a.dim(0).set_bounds(0, J).set_stride(1);
    a.dim(1).set_bounds(0, K).set_stride(J);
    a.dim(2).set_bounds(0, BATCH).set_stride(K*J);

    Var  P;
    // UREs. All are recursive functions, and need signatures to be declared. Two exceptions are Q and R, functions
    // for the final results, which are not really recursive Funcs, and declaring their places is enough.
    FuncType vec_a("vec_a", TYPE, {P}, Place::Device), 
             vec_ai("vec_ai", TYPE, {P}, Place::Device), 
             vec_t("vec_t", TYPE, {P}, Place::Device), 
             A("A", TYPE, {P}, Place::Device), 
             Q("Q", Place::Device), 
             vec_ti("vec_ti", TYPE, {P}, Place::Device), 
             X("X", TYPE, {P}, Place::Device),
             Xi("Xi", TYPE, {P_no_k}, Place::Device),
             irXi("irXi", TYPE, {P_no_k}, Place::Device),
             S("S", TYPE, {P_no_k}, Place::Device),
             R("R", Place::Device);
    vec_a(k, j, i, b) = select(i < 0, a(j, k, b), A(k, j, i - 1, b));
    vec_ai(k, j, i, b) = select(j == i, vec_a(k, j, i, b), vec_ai(k, j - 1, i, b));
    vec_t(k, j, i, b) = select(j == i, 0, vec_a(k, j, i, b)) + select(i < 0, 0, S(j, i - 1, b)) * vec_ai(k, j, i, b);
    A(k, j, i, b) = vec_t(k, j, i, b);
    Q(k, j, b) = select(j == i, A(k, j, i, b));   // Output matrix Q
    vec_ti(k, j, i, b) = select(j == i + 1, vec_t(k, j, i, b), vec_ti(k, j - 1, i, b));
    X(k, j, i, b) = select(k == 0, 0, X(k - 1, j, i, b)) + vec_ti(k, j, i, b) * vec_t(k, j, i, b); // innner product
    Xi(j, i, b) = select(j == i + 1, X(K - 1, j, i, b), Xi(j - 1, i, b));
    irXi(j, i, b) = select(j == i + 1, fast_inverse_sqrt(X(K - 1, j, i, b)), irXi(j - 1, i, b));
    S(j, i, b) = select(j == i + 1, irXi(j, i, b), -X(K - 1, j, i, b)/Xi(j, i, b));
    R(j, i, b) = select(i < I - 1 && j > i, select(j == i + 1, sqrt(Xi(j, i, b)), irXi(j, i, b) * X(K - 1, j, i, b))); // Output matrix R

    // Put all the UREs inside the same loop nest of vec_a.
    vec_a.merge_ures({j, j, j, k, j, j, j, j, j, j}, {vec_ai, vec_t, A, Q, vec_ti, X, Xi, irXi, S, R}, {Q, R});
    // Explicitly set the loop bounds
    vec_a.set_bounds(b, 0, BATCH, i, -1, I + 1, j, max(0, i), J - max(0, i), k, 0, K);
    vec_a.space_time_transform(k);
    vec_a.triangular_loop_optimize(i, j, 64);


    FuncType ASerializer("ASerializer", Place::Host),
             AFeeder("AFeeder", Place::Device);
    FuncType QCollector("QCollector", Place::Device), 
             RUnloader("RUnloader", Place::Device),
             RDeserializer("RDeserializer", Place::Host), QDeserializer("QDeserializer", Place::Host);

    vec_a.isolate_producer_chain(a, ASerializer, AFeeder);
    ASerializer.set_bounds(b, 0, BATCH, i, -1, 1, j, 0, J, k, 0, K);
    AFeeder.set_bounds(b, 0, BATCH, i, -1, 1, j, 0, J, k, 0, K);
    AFeeder.scatter(ASerializer, k, ScatterStrategy::FPGAReg);

    Q.isolate_consumer(QCollector);
    QCollector.space_time_transform(k);
    QCollector.set_bounds(b, 0, BATCH, j, 0, J, k, 0, K);
    QCollector.gather(Q, k, GatherStrategy::FPGAReg);
    QCollector.isolate_consumer_chain(QDeserializer);

    R.isolate_consumer_chain(RUnloader, RDeserializer);
    RUnloader.set_bounds(b, 0, BATCH, i, 0, I, j, i, J - i);
    RDeserializer.set_bounds(b, 0, BATCH, i, 0, I, j, 0, J);

    AFeeder.late_fuse(vec_a, b, 4);
    QCollector.late_fuse(vec_a, b, 4);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    // Buffer<float> resultQ(K, J, BATCH_SIZE);
    // Buffer<float> resultR(J, I, BATCH_SIZE);
    // Pipeline({QDeserializer, RDeserializer}).realize({resultQ, resultR}, target);
    // resultR.copy_to_host();
    // resultQ.copy_to_host();
    Pipeline({QDeserializer, RDeserializer}).compile_jit(target);
    
    cout << "Success!\n";
    return 0;
}
