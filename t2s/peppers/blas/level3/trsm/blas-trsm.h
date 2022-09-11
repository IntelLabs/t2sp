#ifndef blas_trsm_h
#define blas_trsm_h

#include <assert.h>
#include <iostream>

#include "const-parameters.h"

using namespace std;

// use template
template<typename T>
void trsm(bool side, bool uplo, bool trans, bool diag, int m, int n, T alpha, T *a, int lda, T *x, int ldx) {
    assert(lda >= n);

    T *aCopy = new T[m * m];
    T *xCopy = new T[m * n];
    for (int iterI = 0; iterI < m; iterI++) {
        for (int iterK = 0; iterK < n; iterK++) {
            xCopy[iterI * n + iterK] = x[iterI * n + iterK];
        }
    }

    for (int iterI = 0; iterI < n; iterI++) {
        for (int iterJ = 0; iterJ < n; iterJ++) {
            int rowIdx = (iterI >= iterJ) == (uplo == UPLO_LO) ? iterI : iterJ;
            int colIdx = (iterI >= iterJ) == (uplo == UPLO_LO) ? iterJ : iterI;
            int stride = trans ? m : lda;
            aCopy[iterI * m + iterJ] = (iterI <= iterJ) ? 0 : aCopy[rowIdx * stride + colIdx];
        }
    }

    for (int iterK = 0; iterK < n; iterK++)
        for (int iterI = 0; iterI < m; iterI++) {
            for (int iterJ = 0; iterJ <= iterI; iterJ++)
                x[iterI * n + iterK] -= a[iterI * m + iterJ] * xCopy[iterJ * n + iterK];
            x[iterI * n + iterK] = -x[iterI * n + iterK];
        }
}

#endif

