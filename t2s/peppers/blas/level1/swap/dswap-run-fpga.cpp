#include "blas-swap.h"
#include "const-parameters.h"

#define I 4

#include <stdio.h>
#include <iostream>
#include <assert.h>

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;

    double *x = new double[TOTAL_I];
    double *y = new double[TOTAL_I];
    double *x_copy = new double[TOTAL_I];
    double *y_copy = new double[TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        x[i] = random();
        y[i] = random();
        x_copy[i] = x[i];
        y_copy[i] = y[i];
    }
    
    swap<double>(TOTAL_I, x, 1, y, 1);
  
    for (int i = 0; i < TOTAL_I; i++) {
        assert(abs(x_copy[i] - y[i]) < 0.005*abs(x_copy[i]));
        assert(abs(y_copy[i] - x[i]) < 0.005*abs(y_copy[i]));
    }

    printf("Success\n");
    return 0;
}
