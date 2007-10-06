#include "aux.h"
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_deriv.h>
#include <gsl/gsl_poly.h>
#include <gsl/gsl_multimin.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <string.h>
#include <stdio.h>

#define MACRO(B) do {B} while (0)
#define ERROR(CODE) MACRO(return CODE;)
#define REQUIRES(COND, CODE) MACRO(if(!(COND)) {ERROR(CODE);})
#define OK return 0;

#define MIN(A,B) ((A)<(B)?(A):(B))
#define MAX(A,B) ((A)>(B)?(A):(B))

#ifdef DBG
#define DEBUGMSG(M) printf("*** calling aux C function: %s\n",M);
#else
#define DEBUGMSG(M)
#endif

#define CHECK(RES,CODE) MACRO(if(RES) return CODE;)

#ifdef DBG
#define DEBUGMAT(MSG,X) printf(MSG" = \n"); gsl_matrix_fprintf(stdout,X,"%f"); printf("\n");
#else
#define DEBUGMAT(MSG,X)
#endif

#ifdef DBG
#define DEBUGVEC(MSG,X) printf(MSG" = \n"); gsl_vector_fprintf(stdout,X,"%f"); printf("\n");
#else
#define DEBUGVEC(MSG,X)
#endif

#define DVVIEW(A) gsl_vector_view A = gsl_vector_view_array(A##p,A##n)
#define DMVIEW(A) gsl_matrix_view A = gsl_matrix_view_array(A##p,A##r,A##c)
#define CVVIEW(A) gsl_vector_complex_view A = gsl_vector_complex_view_array((double*)A##p,A##n)
#define CMVIEW(A) gsl_matrix_complex_view A = gsl_matrix_complex_view_array((double*)A##p,A##r,A##c)
#define KDVVIEW(A) gsl_vector_const_view A = gsl_vector_const_view_array(A##p,A##n)
#define KDMVIEW(A) gsl_matrix_const_view A = gsl_matrix_const_view_array(A##p,A##r,A##c)
#define KCVVIEW(A) gsl_vector_complex_const_view A = gsl_vector_complex_const_view_array((double*)A##p,A##n)
#define KCMVIEW(A) gsl_matrix_complex_const_view A = gsl_matrix_complex_const_view_array((double*)A##p,A##r,A##c)

#define V(a) (&a.vector)
#define M(a) (&a.matrix)

#define GCVEC(A) int A##n, gsl_complex*A##p
#define KGCVEC(A) int A##n, const gsl_complex*A##p

#define BAD_SIZE 2000
#define BAD_CODE 2001
#define MEM      2002
#define BAD_FILE 2003

int transR(KRMAT(x),RMAT(t)) {
    REQUIRES(xr==tc && xc==tr,BAD_SIZE);
    DEBUGMSG("transR");
    KDMVIEW(x);
    DMVIEW(t);
    int res = gsl_matrix_transpose_memcpy(M(t),M(x));
    CHECK(res,res);
    OK
}

int transC(KCMAT(x),CMAT(t)) {
    REQUIRES(xr==tc && xc==tr,BAD_SIZE);
    DEBUGMSG("transC");
    KCMVIEW(x);
    CMVIEW(t);
    int res = gsl_matrix_complex_transpose_memcpy(M(t),M(x));
    CHECK(res,res);
    OK
}


int submatrixR(int r1, int r2, int c1, int c2, KRMAT(x),RMAT(r)) {
    REQUIRES(0<=r1 && r1<=r2 && r2<xr && 0<=c1 && c1<=c2 && c2<xc &&
            rr==r2-r1+1 && rc==c2-c1+1,BAD_SIZE);
    DEBUGMSG("submatrixR");
    KDMVIEW(x);
    DMVIEW(r);
    gsl_matrix_const_view S = gsl_matrix_const_submatrix(M(x),r1,c1,rr,rc);
    int res = gsl_matrix_memcpy(M(r),M(S));
    CHECK(res,res);
    OK
}


int constantR(double * pval, RVEC(r)) {
    DEBUGMSG("constantR")
    int k;
    double val = *pval;
    for(k=0;k<rn;k++) {
        rp[k]=val;
    }
    OK
}

int constantC(gsl_complex* pval, CVEC(r)) {
    DEBUGMSG("constantC")
    int k;
    gsl_complex val = *pval;
    for(k=0;k<rn;k++) {
        rp[k]=val;
    }
    OK
}


int multiplyR(int ta, KRMAT(a), int tb, KRMAT(b),RMAT(r)) {
    //printf("%d %d %d %d %d %d\n",ar,ac,br,bc,rr,rc);
    //REQUIRES(ac==br && ar==rr && bc==rc,BAD_SIZE);
    DEBUGMSG("multiplyR (gsl_blas_dgemm)");
    KDMVIEW(a);
    KDMVIEW(b);
    DMVIEW(r);
    int res = gsl_blas_dgemm(
         ta?CblasTrans:CblasNoTrans,
         tb?CblasTrans:CblasNoTrans,
         1.0, M(a), M(b),
         0.0, M(r));
    CHECK(res,res);
    OK
}

int multiplyC(int ta, KCMAT(a), int tb, KCMAT(b),CMAT(r)) {
    //REQUIRES(ac==br && ar==rr && bc==rc,BAD_SIZE);
    DEBUGMSG("multiplyC (gsl_blas_zgemm)");
    KCMVIEW(a);
    KCMVIEW(b);
    CMVIEW(r);
    gsl_complex alpha, beta;
    GSL_SET_COMPLEX(&alpha,1.,0.);
    GSL_SET_COMPLEX(&beta,0.,0.);
    int res = gsl_blas_zgemm(
         ta?CblasTrans:CblasNoTrans,
         tb?CblasTrans:CblasNoTrans,
         alpha, M(a), M(b),
         beta, M(r));
    CHECK(res,res);
    OK
}


int diagR(KRVEC(d),RMAT(r)) {
    REQUIRES(dn==rr && rr==rc,BAD_SIZE);
    DEBUGMSG("diagR");
    int i,j;
    for (i=0;i<rr;i++) {
        for(j=0;j<rc;j++) {
            rp[i*rc+j] = i==j?dp[i]:0.;
        }
    }
    OK
}

int diagC(KCVEC(d),CMAT(r)) {
    REQUIRES(dn==rr && rr==rc,BAD_SIZE);
    DEBUGMSG("diagC");
    int i,j;
    gsl_complex zero;
    GSL_SET_COMPLEX(&zero,0.,0.);
    for (i=0;i<rr;i++) {
        for(j=0;j<rc;j++) {
            rp[i*rc+j] = i==j?dp[i]:zero;
        }
    }
    OK
}