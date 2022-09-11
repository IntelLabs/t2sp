#include "blas-trsv.h"
#include "const-parameters.h"

#define I 4

#include <stdio.h>
#include <iostream>
#include <assert.h>

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;

    double *a = new double[TOTAL_I * TOTAL_I];
    double *x = new double[TOTAL_I];
    double *b = new double[TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_I; j++) {
            if (i >= j) a[i * TOTAL_I + j] = random();
            else a[i * TOTAL_I + j] = 0;
        }
    }

    for (size_t i = 0; i < TOTAL_I; i++) {
        x[i] = random();
        b[i] = x[i];
    }
    
    trsv<double>(UPLO_LO, false, false, TOTAL_I, a, TOTAL_I, x, 1);

    for (int i = 0; i < TOTAL_I; i++) {
        double golden = 0;
        for (int j = 0; j < TOTAL_I; j++) golden += a[i * TOTAL_I + j] * b[j];
        assert(abs(golden - x[i]) < 0.005*abs(golden));
    }

    printf("Success\n");
    return 0;
}
