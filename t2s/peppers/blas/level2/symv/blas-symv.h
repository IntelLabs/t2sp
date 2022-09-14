#ifndef blas_symv_h
#define blas_symv_h

#include <assert.h>

#include "const-parameters.h"

// use template
template<typename T>
void symv(bool layout, bool trans,
          int m, int n, T alpha, T *a, int lda,
          T *x, int incx, T beta,
          T *y, int incy) {
    
    assert((trans && lda >= m) || (!trans && lda >= n));
    assert(incy >= 1);

    const int I = (m + III * II - 1) / (III * II);
    const int J = (n + JJJ * JJ - 1) / (JJJ * JJ);

    for (int iterI = 0; iterI < m; iterI++)
        for (int iterJ = 0; iterJ < n; iterJ++)
            y[iterI * incy] += alpha * a[iterI * n + iterJ] * x[iterJ * incx];
}

#endif
