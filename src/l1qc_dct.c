#include "config.h"
#include "l1c_timing.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "l1c_common.h"
#include "l1c_memory.h"

#include "l1qc_newton.h"
#include "omp.h"

/* dct_mkl.h defines the Ax and Aty operations.
   To adapt this file to a different set of transformations,
   this largely what must be changed. Your new set of transformations
   must expose three functions with the following prototype:

   void Ax(double *x, double *y)
   void Aty(double *y, double *x)
   void AtAx(double *x, double *z)

   where x and z have length n, and y has length m and m<n.
*/

#include "dct.h"

#if defined(_USEMKL_)
#define dct2_EMx dct_EMx
#define dct2_MtEty dct_MtEty
#define dct2_MtEt_EMx dct_MtEt_EMx
#define dct2_idct dct_idct
#define dct2_setup dct_setup_compat
#define  dct2_destroy dct_destroy
#else
#include "dct2.h"
#endif

int dct_setup_compat(l1c_int N, l1c_int M, l1c_int Ny, l1c_int *pix_mask_idx){
  // Placeholder, eventually dct_setup should be common to both 1 and 2d transforms.
  (void) M;
  return dct_setup(N, Ny, pix_mask_idx);
}

typedef struct XFormPack {
  AxFuns Ax_funs;
  void(*M)(double *x);
  // int(*setup)(l1c_int Nx, l1c_int Ny, l1c_int *pix_mask_idx);
  int(*setup)(l1c_int N, l1c_int M, l1c_int Ny, l1c_int *pix_mask_idx);

  // int(*setup)(l1c_int Nrow, l1c_int Ncol, l1c_int M, l1c_int *pix_idx);
  void(*destroy)(void);

}XFormPack;



int get_transform_funcs(int Ncol, XFormPack *xforms){

  if (Ncol > 1){
    xforms-> Ax_funs.Ax=dct2_EMx;
    xforms-> Ax_funs.Aty=dct2_MtEty;
    xforms-> Ax_funs.AtAx=dct2_MtEt_EMx;
    xforms->M = dct2_idct;
    xforms->setup = dct2_setup;
    xforms->destroy = dct2_destroy;
  }else if(Ncol == 1){
    xforms-> Ax_funs.Ax=dct_EMx;
    xforms-> Ax_funs.Aty=dct_MtEty;
    xforms-> Ax_funs.AtAx=dct_MtEt_EMx;
    xforms->M = dct_idct;
    xforms->setup = dct_setup_compat;
    xforms->destroy = dct_destroy;
  }else{
    fprintf(stderr, "Ncol=%d makes no sense.\n", Ncol);
    return 1;
  }

  return 0;

}



typedef struct L1qcDctOpts{
  double epsilon;
  double mu;
  double lbtol;
  double tau;
  int lbiter;
  double newton_tol;
  int newton_max_iter;
  int verbose;
  double l1_tol;
  double cgtol;
  int cgmaxiter;
  int warm_start_cg;
}L1qcDctOpts;




/*--------------------------- Two-dimensional version ------------------------------*/

int l1qc_dct(int Nrow, int Ncol, double *x_out, int M, double *b, l1c_int *pix_idx,
             L1qcDctOpts opts, LBResult *lb_res){
  struct timeval tv_start, tv_end;
  tv_start = l1c_get_time();

  int status = 0;
  int Ntot = Nrow*Ncol;

  XFormPack xforms;

  if (get_transform_funcs(Ncol, &xforms)){
    fprintf(stderr, "Exiting...............\n");
    return 1;
  }
  AxFuns Ax_funs = xforms.Ax_funs;
  /*
    Pointers to the transform functions that define A*x and A^T *y
  */


  NewtParams params = {.epsilon = opts.epsilon,
                       .tau = opts.tau,
                       .mu = opts.mu,
                       .newton_tol = opts.newton_tol,
                       .newton_max_iter = opts.newton_max_iter,
                       .lbiter = 0,
                       .l1_tol = opts.l1_tol,
                       .lbtol = opts.lbtol,
                       .verbose = opts.verbose,
                       .cg_params.max_iter = opts.cgmaxiter,
                       .cg_params.tol = opts.cgtol,
                       .cg_params.verbose=0,
                       .warm_start_cg=opts.warm_start_cg};


  /* Allocate memory for x and b, which is aligned to DALIGN.
     Pointer address from caller probably wont be properly aligned.
   */
  printf("Nrow = %d, Ncol=%d, M=%d\n", Nrow, Ncol, M);
  double *eta_0 = malloc_double(Ntot);
  double *b_ours = malloc_double(M);

  if ( !b_ours || !eta_0){
    fprintf(stderr, "Memory Allocation failure\n");
    status =  L1C_OUT_OF_MEMORY;
    goto exit1;
  }

  cblas_dcopy(M, b, 1, b_ours, 1);

  if (xforms.setup(Nrow, Ncol, M, (l1c_int*)pix_idx)){
    status = L1C_DCT_INIT_FAILURE;
    goto exit1;
  }
  Ax_funs.Aty(b_ours, eta_0);

  *lb_res = l1qc_newton(Ntot, eta_0, M, b_ours, params, Ax_funs);
  if (lb_res->status){
    status = lb_res->status;
    goto exit2;
  }
  /* We solved for eta in the DCT domain. Transform back to
     standard coorbbdinates.
  */
  xforms.M(eta_0);

  cblas_dcopy(Ntot, eta_0, 1, x_out, 1);


  tv_end = l1c_get_time();

  double time_total = l1c_get_time_diff(tv_start, tv_end);

  printf("total c time: %f\n", time_total);
  printf("total-newton-iter: %d\n", lb_res->total_newton_iter);
  printf("total-cg-iter: %d\n", lb_res->total_cg_iter);
  printf("time per cg iter: %g\n", time_total / (double) lb_res->total_cg_iter);

 exit2:
  xforms.destroy(); // Should not call this if ax_setup() failed.
 exit1:
  /* Cleanup our mess. */
  free_double(b_ours);
  free_double(eta_0);
  return status;
}

