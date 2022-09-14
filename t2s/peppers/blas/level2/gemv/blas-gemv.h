#ifndef blas_gemv_h
#define blas_gemv_h

#include <assert.h>

#include "const-parameters.h"
#include "HalideBuffer.h"

// use template
template<typename T>
void gemv(bool layout, bool trans,
          int m, int n, T alpha, T *a, int lda,
          T *x, int incx, T beta,
          T *y, int incy) {

    Halide::Runtime::Buffer<float> bufferA(1, 1), bufferB(1, 1), bufferC(1, 1);
    
    assert((trans && lda >= m) || (!trans && lda >= n));
    assert(incy >= 1);

    const int I = (m + III * II - 1) / (III * II);
    const int J = (n + JJJ * JJ - 1) / (JJJ * JJ);

    for (int iterI = 0; iterI < m; iterI++)
        for (int iterJ = 0; iterJ < n; iterJ++)
            y[iterI * incy] += alpha * a[iterI * n + iterJ] * x[iterJ * incx];
}

#endif
