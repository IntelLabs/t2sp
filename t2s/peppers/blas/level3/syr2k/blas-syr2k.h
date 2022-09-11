#ifndef blas_syr2k_h
#define blas_syr2k_h

#include <assert.h>
#include <iostream>
#include "const-parameters.h"

using namespace std;

// use template
template<class T>
void syr2k(bool layout, bool uplo, bool trans, int n, int k, T alpha, T *a, int lda, T *b, int ldb, T beta, T *c, int ldc) {
    assert(lda >= (trans ? n : k));
    assert(ldb >= (trans ? n : k));
    assert(ldc >= n);

    int aPos, bPos, cSrc, cDst;
    T *newC = new T[n * ldc];
    T *newC2 = new T[n * ldc];
    for (int iterI = 0; iterI < n; iterI++)
        for (int iterJ = 0; iterJ < n; iterJ++) {
            cDst = (layout == LAYOUT_ROW) ? (iterI * ldc + iterJ) : (iterJ * ldc + iterI);
            newC[cDst] = 0;
            for (int iterK = 0; iterK < k; iterK++) {
                aPos = (trans == layout) ? (iterI * lda + iterK) : (iterK * lda + iterI);
                bPos = (trans == layout) ? (iterJ * ldb + iterK) : (iterK * ldb + iterJ);
                newC[cDst] += a[aPos] * b[bPos];
            }
        }
        
    for (int iterI = 0; iterI < n; iterI++)
        for (int iterJ = 0; iterJ < n; iterJ++) {
            cDst = (layout == LAYOUT_ROW) ? (iterI * ldc + iterJ) : (iterJ * ldc + iterI);
            cSrc = (layout == LAYOUT_ROW) ? (iterJ * n + iterI) : (iterI * n + iterJ);
            bool readTrans = (uplo == UPLO_UP && iterI >= iterJ) || (uplo == UPLO_LO && iterI <= iterJ);
            newC2[cDst] = alpha * newC[cDst] + alpha * newC[cSrc] + beta * c[readTrans ? cSrc : cDst];
        }

    for (int iterI = 0; iterI < n; iterI++)
        for (int iterJ = 0; iterJ < n; iterJ++) {
            cDst = (layout == LAYOUT_ROW) ? (iterI * ldc + iterJ) : (iterJ * ldc + iterI);
            c[cDst] = newC2[cDst];
        }
}

#endif
