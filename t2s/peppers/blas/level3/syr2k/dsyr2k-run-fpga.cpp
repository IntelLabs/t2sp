#include <stdio.h>
#include <iostream>
#include <assert.h>
#include "blas-syr2k.h"
#include "const-parameters.h"

#define I 4
#define J 4

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;
    const int TOTAL_J = JJJ * JJ * J;

    double alpha = random(), beta = random();
    double *a = new double[TOTAL_I*TOTAL_J];
    double *b = new double[TOTAL_I*TOTAL_J];
    double *c = new double[TOTAL_I*TOTAL_I];
    double *c_copy = new double[TOTAL_I*TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            a[i * TOTAL_I + j] = random();
            b[i * TOTAL_I + j] = random();
        }
    }

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_I; j++) {
            if (i <= j) c[i * TOTAL_I + j] = random();
            else c[i * TOTAL_I + j] = c[j * TOTAL_I + i];
            c_copy[i * TOTAL_I + j] = c[i * TOTAL_I + j];
        }
    }
    
    syr2k<double>(LAYOUT_ROW, UPLO_UP, false, TOTAL_I, TOTAL_J, alpha, a, TOTAL_J, b, TOTAL_J, beta, c, TOTAL_I);

    for (int i = 0; i < TOTAL_I; i++)
        for (int j = 0; j < TOTAL_I; j++) {
            double golden = 0;
            for (int k = 0; k < TOTAL_J; k++) {
                golden += a[i * TOTAL_J + k] * b[j * TOTAL_J + k] + a[j * TOTAL_J + k] * b[i * TOTAL_J + k];
            }
                
            golden = alpha * golden + beta * c_copy[i * TOTAL_I + j];
            assert(abs(golden - c[i * TOTAL_I + j]) < 0.005*abs(golden));
        }

    printf("Success\n");
    return 0;
}
