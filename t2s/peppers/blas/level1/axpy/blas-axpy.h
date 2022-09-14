#ifndef blas_axpy_h
#define blas_axpy_h

#include <assert.h>

#include "const-parameters.h"

// use template
template<typename T>
void axpy(int n, T a, T *x, int incx, T *y, int incy, T *out) {
    assert(incy >= 1);
    const int I = (n + III * II - 1) / (III * II);

    for (int iterI = 0; iterI < n; iterI++) {
        out[iterI] += a * x[iterI * incx] + y[iterI * incy];
    }
}

#endif
