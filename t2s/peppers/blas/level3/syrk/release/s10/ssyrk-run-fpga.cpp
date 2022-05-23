#include "blas-syrk.h"
#include "const-parameters.h"

#include <stdio.h>
#include <iostream>
#include <assert.h>

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;
    const int TOTAL_K = KKK * KK * K;

    char uplo = 'U';
    char trans = 'N';

    float alpha = random();
    float beta = random();

    float *a = new float[TOTAL_I * TOTAL_K];
    float *c = new float[TOTAL_I * TOTAL_I];
    float *cCopy = new float[TOTAL_I * TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t k = 0; k < TOTAL_K; k++) {
            a[i * TOTAL_K + k] = random();
        }
    }
    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = i; j < TOTAL_I; j++) {
            c[i * TOTAL_I + j] = random();
            cCopy[i * TOTAL_I + j] = c[i * TOTAL_I + j];
            cCopy[j * TOTAL_I + i] = c[i * TOTAL_I + j];
        }
    }
    
    ssyrk(uplo, trans, TOTAL_I, TOTAL_K, alpha, a, 0, beta, c, 0);

    for (int i = 0; i < TOTAL_I; i++) {
        for (int j = 0; j < TOTAL_I; j++) {
            float golden = beta * cCopy[i * TOTAL_I + j];

            for (int k = 0; k < TOTAL_K; k++) {
                float aa = trans == 'N' ? a[i * TOTAL_K + k] : a[k * TOTAL_I + i];
                float bb = trans == 'N' ? a[j * TOTAL_K + k] : a[k * TOTAL_I + j];
                golden += alpha * aa * bb;
            }

            assert(abs(golden - c[i * TOTAL_I + j]) < 0.005*abs(golden));
        }
    }

    printf("Success\n");
    return 0;
}
