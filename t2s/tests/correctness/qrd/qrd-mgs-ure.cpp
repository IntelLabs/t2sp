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
#include "complex.h"

// For printing output
#include <stdio.h>

// For validation of results.
#include <assert.h>

using namespace Halide;
using namespace std;


#define COMPLEX_MATRIX

#ifdef COMPLEX_MATRIX
    #define TYPE     {Float(32), Float(32)}
    #define FuncType ComplexFunc
    #define ZERO     ComplexExpr(0, 0)
    ImageParam a_re(Float(32), 2);
    ImageParam a_im(Float(32), 2);
    #define ajk      ComplexExpr(a_re(j - 1, k), a_im(j - 1, k))
#else
    #define TYPE     Float(32)
    #define FuncType Func
    #define ZERO     0
    ImageParam a(Float(32), 2);
    #define ajk      a(j - 1, k)
#endif
#define I 128
#define J 128
#define K 128

int main(void) {
    // Macros: for convenient use.
    #define P                      k,     j,     i
    #define P_no_k                        j,     i
    #define P_no_j                 k,            i
    #define P_k_minus_1            k - 1, j,     i
    #define P_j_minus_1            k,     j - 1, i
    #define P_i_minus_1            k,     j,     i - 1

    Var  P;
    // UREs. All are recursive functions, and need signatures to be declared. Two exceptions are Q and R, functions
    // for the final results, which are not really recursive Funcs, and declaring their places is enough.
    FuncType A("A", TYPE, {P}, Place::Device), 
             Ai("Ai", TYPE, {P}, Place::Device), 
             X("X", TYPE, {P}, Place::Device), 
             Xi("Xi", TYPE, {P}, Place::Device), 
             Xk("Xk", TYPE, {P}, Place::Device), 
             Xik("Xik", TYPE, {P}, Place::Device), 
             R("R", Place::Device),
             Q("Q", Place::Device);
    Xk(k, j, i) = select(i > 0, select(k == 0, X(k + K - 1, j, i - 1), Xk(k - 1, j, i)), ZERO);
    Xik(k, j, i) = select(i > 0, select(k == 0, Xi(k + K - 1, j, i - 1), Xik(k - 1, j, i)), ZERO);
    A(k, j, i) = select(j > i, select(i == 0, ajk, A(k, j, i - 1) - Xk(k, j, i) / Xik(k, j, i) * Ai(k, j, i - 1)), ZERO);
    Ai(k, j, i) = select(j > i, select(j == i + 1, A(k, j, i), Ai(k, j - 1, i)), ZERO);
    X(k, j, i) = select(j > i, select(k == 0, ZERO, X(k - 1, j, i)) + conj(Ai(k, j, i)) * A(k, j, i), ZERO);
    Xi(k, j, i) = select(j > i, select(j == i + 1, X(k, j, i), Xi(k, j - 1, i)), ZERO);
    R(j, i) = select(k == K - 1 && i > 0, select(j == i, sqrt(re(Xk(k, j, i))), Xk(k, j, i) / sqrt(re(Xik(k, j, i)))));
    Q(k, i) = select(i > 0 && j == i, A(k, j, i - 1) / sqrt(re(Xk(k, j, i))));

    Xk.merge_ures({Xik, A, Ai, X, Xi}, {R, Q});
    Xk.set_bounds(i, 0, I + 1, j, 0, J + 1, k, 0, K);
    // Xk.space_time_transform(k);

#ifdef COMPLEX_MATRIX
    constexpr size_t kRandomSeed = 1138;
    constexpr size_t kRandomMin = 1;
    constexpr size_t kRandomMax = 10;
    srand(kRandomSeed); 
    Buffer<float> input_re(J, K), input_im(J, K);
    for (size_t k = 0; k < K; k++) {
        for (size_t j = 0; j < J; j++) {
            int random_val = rand();
            float random_double = random_val % (kRandomMax - kRandomMin) + kRandomMin;
            input_re(j, k) = random_double;
            int random_val_imag = rand();
            random_double = random_val_imag % (kRandomMax - kRandomMin) + kRandomMin;
            input_im(j, k) = random_double;
        }
    }
    a_re.set(input_re);
    a_im.set(input_im);
#else
    Buffer<float> input(J, K);
    for (int j = 0; j < J; j++) {
        for (int k = 0; k < K; k++) {
            input(j, k) = (j + 1)*(k + 1) + log(k * j + 1);
        }
    }
    a.set(input);
#endif
    
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);

#ifdef COMPLEX_MATRIX
    Buffer<float> resultQ_re(K, I + 1);
    Buffer<float> resultQ_im(K, I + 1);
    Buffer<float> resultR_re(J + 1, I + 1);
    Buffer<float> resultR_im(J + 1, I + 1);
    Pipeline({R, Q}).realize({resultR_re, resultR_im, resultQ_re, resultQ_im}, target);
    resultR_re.copy_to_host();
    resultQ_re.copy_to_host();
    resultR_im.copy_to_host();
    resultQ_im.copy_to_host();
    for (int i = 1; i <= I; i++) {
        for (int j = 1; j <= J; j++) {
            if (j < i) {
                resultR_re(j, i) = 0.0f;
                resultR_im(j, i) = 0.0f;
            }
        }
    }
    void check_complex(const Buffer<float> &resultQ_re, 
                       const Buffer<float> &resultQ_im, 
                       const Buffer<float> &resultR_re, 
                       const Buffer<float> &resultR_im, 
                       const Buffer<float> &input_re, 
                       const Buffer<float> &input_im);
    check_complex(resultQ_re, resultQ_im, resultR_re, resultR_im, input_re, input_im);
#else
    Buffer<float> resultQ(K, I + 1);
    Buffer<float> resultR(J + 1, I + 1);
    Pipeline({R, Q}).realize({resultR, resultQ}, target);
    resultR.copy_to_host();
    resultQ.copy_to_host();
    void check(const Buffer<float> &resultQ, const Buffer<float> &resultR, const Buffer<float> &input);
    check(resultQ, resultR, input);
#endif

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

void check_complex(const Buffer<float> &resultQ_re, 
                   const Buffer<float> &resultQ_im, 
                   const Buffer<float> &resultR_re, 
                   const Buffer<float> &resultR_im, 
                   const Buffer<float> &input_re, 
                   const Buffer<float> &input_im) {
    bool pass = true;
    printf("*** Matrix R: \n");
    for (int i = 1; i <= I; i++) {
        for (int j = 1; j <= J; j++) {
            printf("(%6.2f, %6.2fi)   ", resultR_re(j, i), resultR_im(j, i));
        }
        printf("\n");
    }

    printf("*** Matrix Q: \n");
    for (int k = 0; k < K; k++) {
        for (int i = 1; i <= I; i++) {
            printf("(%6.2f, %6.2fi)   ", resultQ_re(k, i), resultQ_im(k, i));
        }
        printf("\n");
    }

    // check if Q * R can reproduce the inputs
    printf("*** Q * R [Input]\n");
    for (int k = 0; k < K; k++) {
        for (int j = 1; j <= J; j++) {
            float golden_re = 0.0f;
            float golden_im = 0.0f;
            for (int i = 1; i <= I; i++) {
                golden_re += resultR_re(j, i) * resultQ_re(k, i) - resultR_im(j, i) * resultQ_im(k, i);
                golden_im += resultR_re(j, i) * resultQ_im(k, i) + resultR_im(j, i) * resultQ_re(k, i);
            }
            bool correct = fabs(input_re(j - 1, k) - golden_re) < 0.005 && fabs(input_im(j - 1, k) - golden_im) < 0.005;
            pass = pass && correct;
            printf("(%6.2f, %6.2fi)%s[%6.2f, %6.2fi]   ", golden_re, golden_im, correct ? " " : " !", input_re(j - 1, k), input_im(j - 1, k));
        }
        printf("\n");
    }
    assert(pass);
}
