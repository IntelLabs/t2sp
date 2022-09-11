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

    float a = random();
    float *x = new float[TOTAL_I];
    float *y = new float[TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        x[i] = random();
        y[i] = random();
    }
    
    float *out = new float[TOTAL_I];
    axpy<float>(TOTAL_I, a, x, 1, y, 1, out);

  
    for (int i = 0; i < TOTAL_I; i++) {
        float golden = a * x[i] + y[i];
        assert(abs(golden - out[i]) < 0.005*abs(golden));
    }

    printf("Success\n");
    return 0;
}
