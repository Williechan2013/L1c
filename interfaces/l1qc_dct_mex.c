#include "mex.h"
#include "matrix.h"
#include <string.h>
#include <stdlib.h>
#include "l1qc_newton.h"
#include "l1c_common.h"
#include <math.h>

/* dct_mkl.h defines the Ax and Aty operations.
   To adapt this mex file to a different set of transformations,
   this largely what must be changed. Your new set of transformations
   must expose three functions with the following prototype:

   void Ax(double *x, double *y)
   void Aty(double *y, double *x)
   void AtAx(double *x, double *z)

   where x and z have length n, and y has length m and m<n.
 */
#include "dct_mkl.h"
#define NUMBER_OF_FIELDS(ST) (sizeof(ST)/sizeof(*ST))

/*
 *	m e x F u n c t i o n

 The matlab protype is
 [x, LBRes] = l1qc_dct(x0, b, pix_idx, opts),
 where
 x0 has length n,
 b has length m
 pix_idx has length m
 and opts is an options struct. See the l1qc_dct.m doc string for details.
 */
void  mexFunction( int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[] )
{
  /* Ensure intel doesnt fuck us.*/
  mkl_set_interface_layer(MKL_INTERFACE_ILP64);
  mkl_set_threading_layer(MKL_THREADING_GNU);

  // l1qc(x0, b, pix_idx, params);
  /* inputs */
  AxFuns Ax_funs = {.Ax=dctmkl_EMx_new,
                    .Aty=dctmkl_MtEty,
                    .AtAx=dctmkl_MtEt_EMx_new};

  double *x_theirs=NULL, *x_ours=NULL,  *b=NULL;
  NewtParams params = {.epsilon=0, .tau=0, .mu=0,
                        .newton_tol=0, .newton_max_iter = 0, .lbiter=0,
                        .lbtol=0, .verbose = 0, .cg_params.tol=0};
  LBResult lb_res = {.status = 0, .total_newton_iter = 0, .l1=INFINITY};

  l1c_int i=0,N=0, M=0, npix=0;
  double *pix_idx_double=NULL, *x_out=NULL;
  l1c_int *pix_idx;
  // mwSize *dims;

  if(nrhs != 4) {
    mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:nrhs",
                      "four inputs required.");
  }

  if( !(nlhs > 0)) {
    mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:nlhs",
                      "One output required.");
  }

  /* -------- Check x0 -------------*/
  if( !mxIsDouble(prhs[0]) ) {
      mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:notDouble",
                      "First Input vector must be type double.");
  }

  /* check that first input argument is a row or column vector */
  if( (mxGetN(prhs[0]) > 1)  & (mxGetM(prhs[0]) >1) ){
    printf("num dim = %d\n", mxGetNumberOfDimensions(prhs[1]));
    mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:notVector",
                      "First Input must be a vector.");
  }else{
    N =(l1c_int)( mxGetM(prhs[0]) *  mxGetN(prhs[0]) );
  }

  /* -------- Check b -------------*/
  if( !mxIsDouble(prhs[1]) ) {
    mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:notDouble",
                      "Second Input vector must be type double.");
  }

  /* check that second input argument is vector */
  if( (mxGetN(prhs[1]) > 1)  & (mxGetM(prhs[1]) >1) ){
    printf("num dim = %d\n", mxGetNumberOfDimensions(prhs[1]));
    mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:notVector",
                      "Second Input must be a vector.");
  }else{
    M = (l1c_int) ( mxGetM(prhs[1]) *  mxGetN(prhs[1]) );
  }


  /* -------- Check pix_idx -------------*/
  if( !mxIsDouble(prhs[2]) ) {
    mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:notDouble",
                      "pix_idx vector must be type double.");
  }

  /* check that pix_idx input argument is vector */
  if( (mxGetN(prhs[2]) > 1)  & (mxGetM(prhs[2]) >1) ){
    printf("num dim = %d\n", mxGetNumberOfDimensions(prhs[1]));
    mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:notVector",
                      "Third Input must be a vector.");
  }else{
    npix = (l1c_int) ( mxGetM(prhs[2]) *  mxGetN(prhs[2]) );
  }


 if ( !(N > M) | (M != npix) ){
   printf("N = %d, M=%d, npix = %d\n", N, M, npix);
   mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:incompatible_dimensions",
                     "Must have length(x0) > length(b), and length(b) = length(pix_idx).");
 }

 if ( !mxIsStruct(prhs[3])){
   mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:notStruct",
                     "params must be a struct.");
 }


  int nfld = mxGetNumberOfFields(prhs[3]);
  // char *flds[]={ "verbose", "tau", "mu"};
  const char *name;

  mxArray *tmp;
  for (i=0; i<nfld; i++){
    // tmp = mxGetField(prhs[3], 0, "verbose");
    tmp = mxGetFieldByNumber(prhs[3], 0, i);
    name =mxGetFieldNameByNumber(prhs[3], i);
    if (!tmp){
      mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:norverbose",
                        "Error loading field '%s'.", name);
    }else if( !mxIsScalar(tmp) | !mxIsNumeric(tmp) ){
        mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:norverbose",
                          "Bad data in field '%s'. Fields in params struct must be numeric scalars.", name);
    }

    if ( strcmp(name, "epsilon") == 0){
      params.epsilon = mxGetScalar(tmp);
    }else if ( strcmp(name, "mu") == 0){
      params.mu = mxGetScalar(tmp);
    }else if ( strcmp(name, "tau") == 0){
      params.tau = mxGetScalar(tmp);
    }else if ( strcmp(name, "newton_tol") == 0){
      params.newton_tol = mxGetScalar(tmp);
    }else if ( strcmp(name, "newton_max_iter") == 0){
      params.newton_max_iter = (l1c_int) mxGetScalar(tmp);
    }else if ( strcmp(name, "lbiter") == 0){
      params.lbiter = (l1c_int)mxGetScalar(tmp);
    }else if ( strcmp(name, "lbtol") == 0){
      params.lbtol = mxGetScalar(tmp);
    }else if ( strcmp(name, "l1_tol") == 0){
      params.l1_tol = mxGetScalar(tmp);
    }else if ( strcmp(name, "cgtol") == 0){
      params.cg_params.tol = mxGetScalar(tmp);
    }else if ( strcmp(name, "cgmaxiter") == 0){
      params.cg_params.max_iter = mxGetScalar(tmp);
    }else if ( strcmp(name, "warm_start_cg") == 0){
      params.warm_start_cg = (int) mxGetScalar(tmp);
    }else if ( strcmp(name, "verbose") == 0){
      params.verbose= (l1c_int) mxGetScalar(tmp);
    }else{
      mexErrMsgIdAndTxt("l1qc:l1qc_log_barrier:norverbose",
                        "Unrecognized field '%s' in params struct.", name);
    }

  }

  if (params.verbose > 1){
    printf("Input Parameters\n---------------\n");
    printf("   verbose:         %d\n", params.verbose);
    printf("   epsilon:         %.5e\n", params.epsilon);
    printf("   tau:             %.5e\n", params.tau);
    printf("   mu:              %.5e\n", params.mu);
    printf("   newton_tol:      %.5e\n", params.newton_tol);
    printf("   newton_max_iter: %d\n", params.newton_max_iter);
    printf("   lbiter:          %d\n", params.lbiter);
    printf("   lbtol:           %.5e\n", params.lbtol);
    printf("   l1_tol:           %.5e\n", params.l1_tol);
    printf("   cgmaxiter:       %d\n", params.cg_params.max_iter);
    printf("   cgtol:           %.5e\n", params.cg_params.tol);

    printf("NB: lbiter and tau usually generated automatically.\n");
  }

  x_theirs = mxGetPr(prhs[0]);
  b = mxGetPr(prhs[1]);
  pix_idx_double = mxGetPr(prhs[2]);

  /* We are going to change x, so we must allocate and make a copy, so we
     dont change data in Matlabs workspace.
  */
  x_ours = malloc_double(N);

  for (i=0; i<N; i++){
    x_ours[i] = x_theirs[i];
  }

  /*
    pix_idx will naturally be a double we supplied from matlab
    and will have 1-based indexing. Convert to integers and
    shift to 0-based indexing.
   */
  pix_idx = calloc(M, sizeof(l1c_int));
  for (i=0; i<M; i++){
    pix_idx[i] = ((l1c_int) pix_idx_double[i]) - 1;
  }

  /*
    Initialize our Ax,Aty, and AtAx transforms. This will allocate memory
    and file-scoped variables in dct_mkl.c. On exit, we deallocate with
    dctmkl_destroy().
   */
  dctmkl_setup(N, M, pix_idx);
  lb_res = l1qc_newton(N, x_ours, M, b, params, Ax_funs);
  dctmkl_destroy();


  /* Prepare output data.
   */
  plhs[0] = mxCreateDoubleMatrix((mwSize)N, 1, mxREAL);

  x_out =  mxGetPr(plhs[0]);

  for (i=0; i<N; i++){
    x_out[i] = x_ours[i];
  }

  /* Only build the output struct if there is more than 1 output.*/
  if (nlhs == 2){
    const char *fnames[] = {"l1",
                            "total_newton_iter",
                            "total_cg_iter",
                            "status"};

    mxArray *l1_mex_pr, *total_newton_iter_mex_pr;
    mxArray *total_cg_iter_mex_pr, *status_mex_pr;
    plhs[1] = mxCreateStructMatrix(1, 1, NUMBER_OF_FIELDS(fnames), fnames);

    l1_mex_pr = mxCreateDoubleMatrix(1,1, mxREAL);
    total_newton_iter_mex_pr = mxCreateDoubleMatrix(1,1, mxREAL);
    total_cg_iter_mex_pr = mxCreateDoubleMatrix(1,1, mxREAL);
    status_mex_pr            = mxCreateDoubleMatrix(1,1, mxREAL);

    *mxGetPr(l1_mex_pr) = lb_res.l1;
    *mxGetPr(total_newton_iter_mex_pr) = (double)lb_res.total_newton_iter;
    *mxGetPr(total_cg_iter_mex_pr) = (double)lb_res.total_cg_iter;
    *mxGetPr(status_mex_pr) = (double)lb_res.status;

    mxSetField(plhs[1], 0, "l1", l1_mex_pr);
    mxSetField(plhs[1], 0, "total_newton_iter", total_newton_iter_mex_pr);
    mxSetField(plhs[1], 0, "total_cg_iter", total_cg_iter_mex_pr);
    mxSetField(plhs[1], 0, "status", status_mex_pr);
  }

  free(pix_idx);

  mkl_free_buffers();

} /* ------- mexFunction ends here ----- */
