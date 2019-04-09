#include "config.h"

#include "cblas.h"
#include "l1c.h"

/* Forward Declarations */
static void Ax(double *x, double *y);
static void Aty(double *y, double *x);
static void AtAx(double *x_in, double *x_out);
static void destroy_matrix_transforms(void);

double *xfm_A=NULL;
l1c_int xfm_N=0;
l1c_int xfm_M=0;
double *xfm_dwork=NULL;


/**
 * Setup a set of transformations for an arbitrary matrix `A`.
 * For the l1 problems to make sense, `n<<m`.
 *
 * @param n Number of rows in the matrix A.
 * @param m Number of columns in the matrix A.
 * @param A An array of doubles with length `n*m`, stored in row
 * major order.
 * @param ax_funs Pointer an L1cAxFuns struct. On succesfful exit, the fields
 * `Ax`, `Aty`, AtAx`, and destroy will be populated.
 *
 * @warning   Do not free `A` until you are done.
 *
 *
 * @warning Do not free A until you are done.
 */
int l1c_setup_matrix_transforms(l1c_int n, l1c_int m, double *A, l1c_AxFuns *ax_funs){

  int L = n > m ? n : m;

  xfm_dwork = l1c_malloc_double(L);
  if (!xfm_dwork){
    return L1C_OUT_OF_MEMORY;
  }
  for (int i=0; i<L; i++){
    xfm_dwork[i] = 0;
  }

  xfm_A = A;
  xfm_N = n;
  xfm_M = m;

  ax_funs->Ax = Ax;
  ax_funs->Aty = Aty;
  ax_funs->AtAx = AtAx;
  ax_funs->destroy = destroy_matrix_transforms;
  ax_funs->M=NULL;
  ax_funs->Mt=NULL;
  ax_funs->E=NULL;
  ax_funs->Et=NULL;
  ax_funs->data=NULL;

  return 0;
}

static void destroy_matrix_transforms(void){
  l1c_free_double(xfm_dwork);
}

/**
   Computes the matrix-vector product y = A * b, for a full matrix A.
   This is a wrapper for cblas_dgemv.

*/
static void Ax(double *x, double *y){
  const double alp = 1.0;
  const double beta = 0.0;
  const l1c_int inc = 1;
  /*For Layout = CblasRowMajor, the value of lda must be at least max(1, m).

    y := alpha*A*x + beta*y

  */
  cblas_dgemv(CblasRowMajor, CblasNoTrans, xfm_N, xfm_M, alp, xfm_A, xfm_M, x, inc, beta, y, inc);
}


/**
   Computes the matrix-vector product x = A^T * y, for a full matrix A.

   This is a wrapper for cblas_dgemv.

*/
static void Aty(double *y, double *x){
  const double alp = 1.0;
  const double beta = 0.0;
  const l1c_int inc = 1;
  /* dgemv computes
     x = alpha*A^T*y + beta*x, with CblasTrans
   */
  //For Layout = CblasRowMajor, the value of lda must be at least max(1, m).
  cblas_dgemv(CblasRowMajor, CblasTrans, xfm_N, xfm_M, alp, xfm_A, xfm_M, y, inc, beta, x, inc);
}

static void AtAx(double *x_in, double *x_out){
  Ax(x_in, xfm_dwork);
  Aty(xfm_dwork, x_out);
}
