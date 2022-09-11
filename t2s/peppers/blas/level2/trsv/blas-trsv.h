#ifndef blas_trsv_h
#define blas_trsv_h

#include <assert.h>
#include <iostream>

#include "const-parameters.h"

using namespace std;

// use template
template<typename T>
void trsv(bool uplo, bool trans, bool diag, int n, T *a, int lda, T *x, int incx) {
    assert(lda >= n);
    assert(incx >= 1);

    T *aCopy = new T[n * n];
    T *xCopy = new T[n];
    for (int iterI = 0; iterI < n; iterI++) {
        xCopy[iterI] = x[iterI * incx];
        for (int iterJ = 0; iterJ < n; iterJ++) {
            int rowIdx = (iterI >= iterJ) == (uplo == UPLO_LO) ? iterI : iterJ;
            int colIdx = (iterI >= iterJ) == (uplo == UPLO_LO) ? iterJ : iterI;
            int stride = trans ? n : lda;
            aCopy[iterI * n + iterJ] = (iterI <= iterJ) ? 0 : aCopy[rowIdx * stride + colIdx];
        }
    }

    for (int iterI = 0; iterI < n; iterI++) {
        for (int iterJ = 0; iterJ <= iterI; iterJ++)
            x[iterI] -= a[iterI * n + iterJ] * xCopy[iterJ];
        x[iterI] = -x[iterI];
    }
}

#endif

