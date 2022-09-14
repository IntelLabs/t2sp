#include <stdio.h>
#include <iostream>
#include <assert.h>
#include "blas-symm.h"
#include "const-parameters.h"

#define I 4

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;
    const int TOTAL_J = JJJ * JJ * J;

    double alpha = random(), beta = random();
    double *a = new double[TOTAL_I*TOTAL_I];
    double *b = new double[TOTAL_I*TOTAL_J];
    double *c = new double[TOTAL_I*TOTAL_J];
    double *c_copy = new double[TOTAL_I*TOTAL_J];

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_I; j++) {
            a[i * TOTAL_I + j] = random();
        }
    }

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            b[i * TOTAL_I + j] = random();
            c[i * TOTAL_I + j] = random();
            c_copy[i * TOTAL_I + j] = c[i * TOTAL_I + j];
        }
    }
    
    symm<double>(LAYOUT_ROW, SIDE_A_LEFT_B_RIGHT, UPLO_UP, TOTAL_I, TOTAL_J, alpha, a, TOTAL_I, b, TOTAL_J, beta, c, TOTAL_J);

    for (int i = 0; i < TOTAL_I; i++)
        for (int j = 0; j < TOTAL_J; j++) {
            double golden = 0;
            for (int k = 0; k < TOTAL_I; k++)
                golden += a[i * TOTAL_I + k] * b[k * TOTAL_J + j];
                
            golden = alpha * golden + beta * c_copy[i * TOTAL_I + j];
            assert(abs(golden - c[i * TOTAL_I + j]) < 0.005*abs(golden));
        }

    printf("Success\n");
    return 0;
}
