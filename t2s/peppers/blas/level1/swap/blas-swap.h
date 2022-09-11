#ifndef blas_swap_h
#define blas_swap_h

#include "const-parameters.h"

// use template
template<class T>
void swap(int n, T *x, int incx, T *y, int incy) {
    assert(incy >= 1);
    const int I = (n + III * II - 1) / (III * II);
    
    for (int iterI = 0; iterI < n; iterI++) {
        T tmp = x[iterI * incx];
        x[iterI * incx] = y[iterI * incy];
        y[iterI * incy] = tmp;
    }
}

#endif
