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

    float *x = new float[TOTAL_I];
    float *y = new float[TOTAL_I];

    for (size_t i = 0; i < TOTAL_I; i++) {
        x[i] = random();
        y[i] = random();
    }
    
    float out = 0;
    dot<float>(TOTAL_I, x, 1, y, 1, &out);

    float golden = 0;
    for (int i = 0; i < TOTAL_I; i++) golden += x[i] * y[i];
    assert(abs(golden - out) < 0.005*abs(golden));

    printf("Success\n");
    return 0;
}
