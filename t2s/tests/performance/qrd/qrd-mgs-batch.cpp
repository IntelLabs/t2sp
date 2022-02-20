// g++ qrd-mgs.cpp -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DVERBOSE_DEBUG
// env HL_DEBUG_CODEGEN=4 CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 'INTEL_FPGA_OCL_PLATFORM_NAME=Intel(R) FPGA SDK for OpenCL(TM)' 'AOC_OPTION=-march=emulator -board=a10gx -emulator-channel-depth-model=strict ' ./a.out >& 1.txt

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
    Q(k, j, b) = select(i >= 0 && j == i, A(k, j, i, b));   // Output matrix Q
    vec_ti(k, j, i, b) = select(j == i + 1, vec_t(k, j, i, b), vec_ti(k, j - 1, i, b));
    X(k, j, i, b) = select(k == 0, 0, X(k - 1, j, i, b)) + vec_ti(k, j, i, b) * vec_t(k, j, i, b); // innner product
    Xi(j, i, b) = select(j == i + 1, X(K - 1, j, i, b), Xi(j - 1, i, b));
    irXi(j, i, b) = select(j == i + 1, 1 / sqrt(X(K - 1, j, i, b)), irXi(j - 1, i, b));
    S(j, i, b) = select(j == i + 1, irXi(j, i, b), -X(K - 1, j, i, b)/Xi(j, i, b));
    R(j, i, b) = select(i < I - 1 && j > i, select(j == i + 1, sqrt(Xi(j, i, b)), irXi(j, i, b) * X(K - 1, j, i, b))); // Output matrix R

    vec_a.merge_ures({j, j, j, k, j, j, j, j, j, j}, {vec_ai, vec_t, A, Q, vec_ti, X, Xi, irXi, S, R}, {Q, R});
    vec_a.set_bounds(b, 0, BATCH, i, -1, I + 1, j, max(0, i), J - max(0, i), k, 0, K);
    vec_a.space_time_transform(k);
    vec_a.triangular_loop_optimize(i, j, 64);


    FuncType ASerializer("ASerializer", Place::Host), 
             ALoader("ALoader", Place::Device),
             AFeeder("AFeeder", Place::Device);
    FuncType QCollector("QCollector", Place::Device), 
             RUnloader("RUnloader", Place::Device), QUnloader("QUnloader", Place::Device), 
             RDeserializer("RDeserializer", Place::Host), QDeserializer("QDeserializer", Place::Host);

    vec_a.isolate_producer_chain(a, ASerializer, ALoader, AFeeder);
    ASerializer.set_bounds(b, 0, BATCH, i, -1, I + 1, j, 0, J, k, 0, K);
    ALoader.set_bounds(b, 0, BATCH, i, -1, I + 1, j, 0, J, k, 0, K);
    AFeeder.set_bounds(b, 0, BATCH, i, -1, I + 1, j, 0, J, k, 0, K);
    AFeeder.scatter(ALoader, k);

    Q.isolate_consumer(QCollector);
    QCollector.space_time_transform(k);
    QCollector.set_bounds(b, 0, BATCH, j, 0, J, k, 0, K);
    QCollector.gather(Q, k);
    QCollector.isolate_consumer_chain(QUnloader, QDeserializer);

    R.isolate_consumer_chain(RUnloader, RDeserializer);
    RUnloader.set_bounds(b, 0, BATCH, i, 0, I, j, i, J - i);
    RDeserializer.set_bounds(b, 0, BATCH, i, 0, I, j, 0, J);

    AFeeder.late_fuse(vec_a, b);
    QCollector.late_fuse(vec_a, b);


    constexpr size_t kRandomSeed = 1138;
    constexpr size_t kRandomMin = 1;
    constexpr size_t kRandomMax = 10;
    srand(kRandomSeed);
#define BATCH_SIZE 3
    Buffer<float> input(J, K, BATCH_SIZE);
    for (size_t b = 0; b < BATCH_SIZE; b++) {
        for (size_t k = 0; k < K; k++) {
            for (size_t j = 0; j < J; j++) {
                int random_val = rand();
                float random_double = random_val % (kRandomMax - kRandomMin) + kRandomMin;
                input(j, k, b) = random_double;
            }
        }
    }
    a.set(input);
    
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

    // Buffer<float> resultQ(K, J, BATCH_SIZE);
    // Buffer<float> resultR(J, I, BATCH_SIZE);
    // Pipeline({QDeserializer, RDeserializer}).realize({resultQ, resultR}, target);
    // resultR.copy_to_host();
    // resultQ.copy_to_host();
    Pipeline({QDeserializer, RDeserializer}).compile_jit(target);
    
    // void check(const Buffer<float> &resultQ, const Buffer<float> &resultR, const Buffer<float> &input);
    cout << "Success!\n";
    return 0;
}

void check(const Buffer<float> &resultQ, const Buffer<float> &resultR, const Buffer<float> &input) {

    // compute matrix Q and matrix R in C style
    Buffer<float> A(K, J + 1, I + 1), Ai(K, J + 1, I + 1), 
                  R(J + 1, I + 1), Q(K, I + 1), 
                  X(K, J + 1, I + 1), Xi(K, J + 1, I + 1),
                  Xk(K, J + 1, I + 1), Xik(K, J + 1, I + 1);
    for (int i = 1; i <= I; i++) {
        for (int j = 1; j <= J; j++) {
            R(j, i) = 0.0f;
        }
    }

    for (int i = 0; i <= I; i++) {
        for (int j = i; j <= J; j++) {
            for (int k = 0; k < K; k++) {
                if (i > 0) {
                    if (k == 0) {
                        Xk(k, j, i) = X(k + K - 1, j, i - 1);
                        Xik(k, j, i) = Xi(k + K - 1, j, i - 1);
                    } else {
                        Xk(k, j, i) = Xk(k - 1, j, i);
                        Xik(k, j, i) = Xik(k - 1, j, i);
                    }
                }
                if (j == i) {
                    if (i > 0) {
                        if (k == K - 1) {
                            R(j, i) = sqrt(Xk(k, j, i));
                        }
                        Q(k, i) = A(k, j, i - 1) / sqrt(Xk(k, j, i));
                    }
                } else {
                    if (i == 0) {
                        A(k, j, i) = input(j - 1, k);
                    } else {
                        if (k == K - 1) {
                            R(j, i) = Xk(k, j, i) / sqrt(Xik(k, j, i));
                        }
                        A(k, j, i) = A(k, j, i - 1) - Xk(k, j, i) / Xik(k, j, i) * Ai(k, j, i - 1);
                    }

                    if (j == i + 1) {
                        Ai(k ,j, i) = A(k, j, i);
                    } else {
                        Ai(k, j, i) = Ai(k, j - 1, i);
                    }

                    if (k == 0) {
                        X(k, j, i) = Ai(k, j, i) * A(k, j, i);
                    } else {
                        X(k, j, i) = X(k-1, j, i) + Ai(k, j, i) * A(k, j, i);
                    }

                    if (j == i + 1) {
                        Xi(k, j, i) = X(k, j, i);
                    } else {
                        Xi(k, j, i) = Xi(k, j - 1, i);
                    }
                }
            }
        }
    }

    bool pass = true;
    // compare C style results with URE style results
    printf("*** Matrix R: C style results (URE style results)\n");
    for (int i = 1; i <= I; i++) {
        for (int j = 1; j <= J; j++) {
            if (j >= i) {
                bool correct = fabs(R(j, i) - resultR(j, i)) < 0.005;
                pass = pass && correct;
                printf("%6.2f (%6.2f%s)", R(j, i), resultR(j, i), correct ? "" : " !!");
            } else {
                printf("%6.2f (%6.2f%s)", 0.0f, 0.0f, "");
            }
        }
        printf("\n");
    }

    printf("*** Matrix Q: C style results (URE style results)\n");
    for (int k = 0; k < K; k++) {
        for (int i = 1; i <= I; i++) {
            bool correct = fabs(Q(k, i) - resultQ(k, i)) < 0.005;
            pass = pass && correct;
            printf("%6.2f (%6.2f%s)", Q(k, i), resultQ(k, i), correct ? "" : " !!");
        }
        printf("\n");
    }

    // check if Q * R can reproduce the inputs
    printf("*** Q * R (Input)\n");
    for (int k = 0; k < K; k++) {
        for (int j = 1; j <= J; j++) {
            float golden = 0.0f;
            for (int i = 1; i <= I; i++) {
                golden += Q(k, i) * R(j, i);
            }
            bool correct = fabs(input(j - 1, k) - golden) < 0.005;
            pass = pass && correct;
            printf("%6.2f (%6.2f%s)", golden, input(j - 1, k), correct ? "" : " !!");
        }
        printf("\n");
    }
    assert(pass);
}
