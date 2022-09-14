#ifndef blas_symm_h
#define blas_symm_h

#include <assert.h>
#include "const-parameters.h"

// use template
template<class T>
void symm(bool layout, bool side, bool uplo, int m, int n, T alpha, T *a, int lda, T *b, int ldb, T beta, T *c, int ldc) {
    assert(lda >= (side == SIDE_A_LEFT_B_RIGHT ? m : n));
    assert(ldb >= (layout == LAYOUT_ROW ? n : m));
    assert(ldc >= (layout == LAYOUT_ROW ? n : m));

    int a_rows = m, a_cols = m, b_rows = m, b_cols = n, shared_size = m;
    if (side == SIDE_A_RIGHT_B_LEFT) a_rows = n, a_cols = n, b_rows = n, b_cols = m, shared_size = n;

    for (int iterI = 0; iterI < m; iterI++)
        for (int iterJ = 0; iterJ < n; iterJ++) {
            T val = 0;
            for (int iterK = 0; iterK < shared_size; iterK++) {
                int aPos = (side == SIDE_A_LEFT_B_RIGHT) ? (iterI * lda + iterK) : (iterJ * lda + iterK);
                int bPos = (side == SIDE_A_LEFT_B_RIGHT) ? (iterK * ldb + iterJ) : (iterK * ldb + iterI);
                val += a[aPos] * b[bPos];
            }
            
            int cPos = (layout == LAYOUT_ROW) ? (iterI * ldc + iterJ) : (iterJ * ldc + iterI);
            c[cPos] = alpha * val + beta * c[cPos];
        }
}

#endif
