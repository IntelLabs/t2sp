#include "blas-trsm.h"
#include "const-parameters.h"

#define I 4
#define J 4

#include <stdio.h>
#include <iostream>
#include <assert.h>

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;
    const int TOTAL_J = JJJ * JJ * J;

    float alpha = random();
    cout << TOTAL_I * TOTAL_I << " " << TOTAL_I * TOTAL_J << endl;
    float *a = new float[TOTAL_I * TOTAL_I];
    float *x = new float[TOTAL_I * TOTAL_J];
    float *b = new float[TOTAL_I * TOTAL_J];

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_I; j++) {
            if (i >= j) a[i * TOTAL_I + j] = random();
            else a[i * TOTAL_I + j] = 0;
        }
    }

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            x[i * TOTAL_J + j] = random();
            b[i * TOTAL_J + j] = x[i * TOTAL_J + j];
        }
    }
    
    trsm<float>(SIDE_LEFT, UPLO_LO, false, false, TOTAL_I, TOTAL_I, alpha, a, TOTAL_I, x, TOTAL_J);

    for (int k = 0; k < TOTAL_J; k++)
        for (int i = 0; i < TOTAL_I; i++) {
            float golden = 0;
            for (int j = 0; j < TOTAL_I; j++) golden += a[i * TOTAL_I + j] * b[j * TOTAL_J + k];
            assert(abs(golden - x[i * TOTAL_I + k]) < 0.005*abs(golden));
        }

    printf("Success\n");
    return 0;
}
