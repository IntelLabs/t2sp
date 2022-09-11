#include "blas-symv.h"
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
    float beta = random();
    float *a = new float[TOTAL_I * TOTAL_J];
    float *x = new float[TOTAL_J];
    float *y = new float[TOTAL_I];
    float *y_copy = new float[TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            if (i <= j) {
                a[i * TOTAL_J + j] = random();
            } else {
                a[i * TOTAL_J + j] = a[j * TOTAL_I + i];
            }
        }
    }

    for (size_t j = 0; j < TOTAL_J; j++) {
        x[j] = random();
    }

    for (size_t i = 0; i < TOTAL_I; i++) {
        y[i] = random();
        y_copy[i] = y[i];
    }
    
    symv<float>(false, false, TOTAL_I, TOTAL_J, alpha, a, TOTAL_I, x, 1, beta, y, 1);

    for (int i = 0; i < TOTAL_I; i++) {
        float golden = beta * y_copy[i];
        for (int j = 0; j < TOTAL_J; j++) {
            golden += alpha * a[i * TOTAL_J + j] * x[j];
        }
        assert(abs(golden - y[i]) < 0.005*abs(golden));
    }

    printf("Success\n");
    return 0;
}
