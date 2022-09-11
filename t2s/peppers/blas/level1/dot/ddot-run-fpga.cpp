#include "blas-dot.h"
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

    for (size_t i = 0; i < TOTAL_I; i++) {
        x[i] = random();
        y[i] = random();
    }
    
    double out = 0;
    dot<double>(TOTAL_I, x, 1, y, 1, &out);

    double golden = 0;
    for (int i = 0; i < TOTAL_I; i++) golden += x[i] * y[i];
    assert(abs(golden - out) < 0.005*abs(golden));

    printf("Success\n");
    return 0;
}
