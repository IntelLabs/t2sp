#ifndef blas_syrk_h
#define blas_syrk_h

// use template
void ssyrk(char uplo, char transa, int n, int k, float alpha, float *a, int lda, float beta, float *c, int ldc);

#endif
