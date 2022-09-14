#ifndef blas_dot_h
#define blas_dot_h

#include "const-parameters.h"

// use template
template<class T>
void dot(int n, T *x, int incx, T *y, int incy, T *out) {
    assert(incy >= 1);
    const int I = (n + III * II - 1) / (III * II);

    T o = 0;
    for (int iterI = 0; iterI < n; iterI++) {
        o += x[iterI * incx] * y[iterI * incy];
    }
    
    *out = o;
}

#endif
