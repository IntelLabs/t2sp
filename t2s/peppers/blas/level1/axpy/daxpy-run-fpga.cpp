#include "blas-axpy.h"
#include "const-parameters.h"

#define I 4

#include <stdio.h>
#include <iostream>
#include <assert.h>

using namespace std;

int main()
{
    const int TOTAL_I = III * II * I;

    double a = random();
    double *x = new double[TOTAL_I];
    double *y = new double[TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        x[i] = random();
        y[i] = random();
    }
    
    double *out = new double[TOTAL_I];
    axpy<double>(TOTAL_I, a, x, 1, y, 1, out);

  
    for (int i = 0; i < TOTAL_I; i++) {
        double golden = a * x[i] + y[i];
        assert(abs(golden - out[i]) < 0.005*abs(golden));
    }

    printf("Success\n");
    return 0;
}
