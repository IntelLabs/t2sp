#include "blas-gemm.h"
#include "const-parameters.h"

#define K 4
#define J 4
#define I 4

#include <stdio.h>
#include <iostream>
#include <assert.h>

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;
    const int TOTAL_J = JJJ * JJ * J;
    const int TOTAL_K = KKK * KK * K;

    char transa = 'N';
    char transb = 'N';

    double alpha = random();
    float beta = random();

    float *a = new float[TOTAL_I * TOTAL_K];
    float *b = new float[TOTAL_K * TOTAL_J];
    float *c = new float[TOTAL_I * TOTAL_J];
    float *cCopy = new float[TOTAL_I * TOTAL_J];

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t k = 0; k < TOTAL_K; k++) {
            a[i * TOTAL_K + k] = random();
        }
    }
    for (size_t k = 0; k < TOTAL_K; k++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            b[k * TOTAL_J + j] = random();
        }
    }
    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            c[i * TOTAL_J + j] = random();
            cCopy[i * TOTAL_J + j] = c[i * TOTAL_J + j];
        }
    }
    
    sgemm(transa, transb, TOTAL_I, TOTAL_J, TOTAL_K, alpha, a, 0, b, 0, beta, c, 0);

    for (int i = 0; i < TOTAL_I; i++) {
        for (int j = 0; j < TOTAL_J; j++) {
            float golden = beta * cCopy[i * TOTAL_J + j];

            for (int k = 0; k < TOTAL_K; k++) {
                float aa = transa == 'N' ? a[i * TOTAL_K + k] : a[k * TOTAL_I + i];
                float bb = transb == 'N' ? b[k * TOTAL_J + j] : b[j * TOTAL_K + k];
                golden += alpha * aa * bb;
            }

            assert(abs(golden - c[i * TOTAL_J + j]) < 0.005*abs(golden));
        }
    }

    printf("Success\n");
    return 0;
}
