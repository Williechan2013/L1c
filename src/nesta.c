/** @file nesta.c
 *
 * This is an implementation of the l1-specialized version of Nesterovs algorithm
 * described in @cite becker_nesta_2011.
 *
 */

#include <stdlib.h>
#include<cblas.h>
#include <math.h>
#include "l1c.h"
#include "l1c_math.h"
#include "nesta.h"


static inline void _l1c_nesta_Wx(l1c_NestaProb *NP, double *x, double *z){
  if (NP->flags & L1C_ANALYSIS){
    NP->ax_funs.Mx(x, z);
  }else{
    cblas_dcopy(NP->m, x, 1, z, 1);
  }
}

static inline void _l1c_nesta_Wty(l1c_NestaProb *NP, double *z, double *x){
  if (NP->flags & L1C_ANALYSIS){
    NP->ax_funs.Mty(z, x);
  }else{
    cblas_dcopy(NP->m, z, 1, x, 1);
  }
}

static inline void _l1c_nesta_Rx(l1c_NestaProb *NP, double *x, double *y){
  if (NP->flags & L1C_ANALYSIS){
    NP->ax_funs.Ex(x, y);
  }else{
    NP->ax_funs.Ax(x, y);
  }
}

static inline void _l1c_nesta_Rty(l1c_NestaProb *NP, double *y, double *x){
  if (NP->flags & L1C_ANALYSIS){
    NP->ax_funs.Ety(y, x);
  }else{
    NP->ax_funs.Aty(y, x);
  }
}


/* The next three functions basically implement a circular buffer for storing the vector
 containing the last L1C_NESTA_NMEAN values of fx. This is probably over-engineered...
*/

/**
   Returns a new instance of l1c_fmean_fifo, with the vector of
   fvals initialized to zero.
 */
struct l1c_fmean_fifo _l1c_new_fmean_fifo(void){
  double *fvals = malloc(sizeof(double)*L1C_NESTA_NMEAN);

  for (int i=0; i<L1C_NESTA_NMEAN; i++){
    fvals[i] = 0.0;
  }

  struct l1c_fmean_fifo fifo = {.f_vals=fvals,
                     .n_total=0,
                     .next=fvals};
  return fifo;
}

/** Push a new value of fval into the fifo and increment fifo.n_total.
 *
 * @param [in,out] fifo An instance of l1c_fmean_fifo.
 * @param [in] fval the new value of the functional, to be pushed into the fifo.
 */
void _l1c_push_fmeans_fifo(struct l1c_fmean_fifo *fifo, double fval) {

  *fifo->next = fval;

  if (fifo->next + 1 > fifo->f_vals + L1C_NESTA_NMEAN-1){
    fifo->next = fifo->f_vals;
  }else{
    fifo->next++;
  }

  if(fifo->n_total < L1C_NESTA_NMEAN){
    fifo->n_total++;
  }
}

/**
 * Compute the mean of fifo.fval. This is a standard mean,
 * except it is normalized by fifo.n_total, not the length of the buffer.
 * This is eq (3.13) in the paper.
 *
 * @param [in] fifo
 */
double _l1c_mean_fmean_fifo(struct l1c_fmean_fifo *fifo) {

  double mean = 0;
  /* Always run the sum over the whole thing. Unused elements should be zero.*/
  for (int i=0; i < L1C_NESTA_NMEAN; i++){
    mean += fifo->f_vals[i];
  }

  return (mean / (double)fifo->n_total);
}


/**
   Initialize a l1c_NestaProblem struct. Memory will be allocated for all
   arrays.
 */
l1c_NestaProb* _l1c_NestaProb_new(l1c_int n, l1c_int m){

  l1c_NestaProb *NP = malloc(sizeof(l1c_NestaProb));
  if (!NP){
    return NULL;
  }

  *NP = (l1c_NestaProb){.n=n, .m=m, .fx=0.0, .xo=NULL, .xk=NULL, .yk=NULL, .zk=NULL,
                        .Atb=NULL, .gradf=NULL, .gradf_sum=NULL,
                        .dwork1=NULL, .dwork2=NULL, .b=NULL, .ax_funs={0},
                        .sigma=0, .mu=0, .tol=0, .L_by_mu=0, .flags=0};


  NP->xo = l1c_calloc_double(m);
  NP->xk = l1c_calloc_double(m);
  NP->yk = l1c_calloc_double(m);
  NP->zk = l1c_calloc_double(m);
  NP->Atb = l1c_calloc_double(m);
  NP->gradf = l1c_calloc_double(m);
  NP->gradf_sum = l1c_calloc_double(m);
  NP->dwork1 = l1c_calloc_double(m);
  NP->dwork2 = l1c_calloc_double(m);

  if (!NP->xo ||!NP->xk || !NP->yk || !NP->zk
      || !NP->Atb || !NP->gradf || !NP->gradf_sum
      || !NP->dwork1 || !NP->dwork2){
    l1c_free_nesta_problem(NP);
    return NULL;
  }

  return NP;
}

/**
   Release the memory allocated l1c_init_nesta_problem().
   The memory for .b and .ax_funs is not released, since those were not allocated
   by l1c_init_nesta_problem().
 */
void l1c_free_nesta_problem(l1c_NestaProb *NP){
  if (NP){
    l1c_free_double(NP->xo);
    l1c_free_double(NP->xk);
    l1c_free_double(NP->yk);
    l1c_free_double(NP->zk);
    l1c_free_double(NP->Atb);
    l1c_free_double(NP->gradf);
    l1c_free_double(NP->gradf_sum);
    l1c_free_double(NP->dwork1);
    l1c_free_double(NP->dwork2);

    free(NP);
  }
}

/**
 * Implements the projections of yk and zk onto the (primal) feasible set Qp.
 * These are described in in eqs (3.5)-(3.7) and (3.10)-(3.12). Both can be put
 * the common framework of
 *
 * \f{align}{
 *  vk = \textrm{arg } \min_{x\in \Q_p} \frac{L_{\mu}}{2} ||x - xx||_2^2 + \langle g, x\rangle
 * \f}
 *
 * Note that we have droped \f$x_k\f$ from the inner product, because it is a constant,
 * which justifies pushing (in step 3) the sum into the inner product, as is done in
 * in (3.10)
 *
 *
 * @note{By default, will use ax_funs.Ex() and ax_funs.Ety(), i.e., assuming the
 * analysis formulation. To use the synthesis formulation, set ax_funs.Ex=NULL
 * and ax_funs.Ety=NULL, and ax_funs.Ax will be used.}
 *
 * @param [in] NP
 * @param [in] xx
 * @param [in] g
 * @param [out] vk Solution vector
 */
void l1c_nesta_project(l1c_NestaProb *NP, double *xx, double *g, double *vk){

  l1c_int n = NP->n;
  l1c_int m = NP->m;

  double *Aq = NP->dwork2;
  double *AtAq = NP->dwork1;
  double Lmu = NP->L_by_mu;
  double a0=0, a1=0, lambda=0;

  /* Store q in vk.*/
  l1c_daxpy_z(m, (-1.0/Lmu), g, xx, vk);

  _l1c_nesta_Rx(NP, vk, Aq);
  _l1c_nesta_Rty(NP, Aq, AtAq);

  // a0 = Lmu * [ ||Aq - b||/sigma  - 1]
  a0 = l1c_dnrm2_err(n, NP->b, Aq);
  a0 = Lmu * (a0/NP->sigma - 1.0);

  lambda = max(0, a0);

  a1 = lambda / (Lmu + lambda);


  /* We start with vk = q, and will compute
    vk = lambda/Lmu*(1 - a1)*Atb + q - a1*AtAq */

  cblas_daxpy(m, (lambda/Lmu) * (1.0 - a1), NP->Atb, 1, vk, 1);
  cblas_daxpy(m, -a1, AtAq, 1, vk, 1);

}


/**
 * Evaluate the (smoothed) functional and compute the gradient.
 *
 * @param [in,out] NP On exist, NP->gradf and NP->fx will be updated.

 y = Ax = Rx,
 y in R^n
 x in R^m
 u = Wx in R^p
 R (or A) is n by m
 W (or U) is p by m

 */
void l1c_nesta_feval(l1c_NestaProb *NP){
  l1c_int m = NP->m;

  double nrm_u2=0;
  double *Uxk = NP->dwork1;
  double *u = NP->dwork2;

  /* If E and Et are void, we are doing synthesis, otherwise, analysis.
   */
  _l1c_nesta_Wx(NP, NP->xk, Uxk);

  for (int i=0; i<m; i++){
    u[i] = Uxk[i] / max(NP->mu_j, fabs(Uxk[i]));
  }


  nrm_u2 = cblas_dnrm2(m, u, 1);
  nrm_u2 *= nrm_u2;

  NP->fx = cblas_ddot(m, u, 1, Uxk, 1) - 0.5 * NP->mu_j * nrm_u2;

  _l1c_nesta_Wty(NP, u, NP->gradf);

}


/**
 * Populates an l1c_NestaProb instance NP. NP should already be allocated by
 *  l1c_init_nesta_problem().
 *
 * @param [out] NP problem instance.
 * @param [in,out] beta_mu Initial beta such that `beta^n_con * mu0 = mu_final`
 * @param [in,out] beta_tol Factor such such that `beta_tol^n_con * tol0 = tol_final`
 * @param [in,out] n_continue The number of continuation iterations.
 *
 */
int l1c_nesta_setup(l1c_NestaProb *NP, double *beta_mu, double *beta_tol,
                     int n_continue, double *b, l1c_AxFuns ax_funs, double sigma,
                     double mu, double tol, double L, unsigned flags){


  /* Check that flags is consistent with functionality provided by ax_funs. */
  if (flags & L1C_ANALYSIS) {
    if (!ax_funs.Mx || !ax_funs.Mty || !ax_funs.Ex || !ax_funs.Ety)
      return L1C_INCONSISTENT_ARGUMENTS;
  }
  NP->b = b;
  NP->n_continue = n_continue;
  NP->ax_funs = ax_funs;
  NP->sigma = sigma;
  NP->mu = mu;
  NP->L_by_mu = L/mu;
  NP->tol = tol;
  NP->flags = flags;

  double mu_final = mu;
  double tol_final = tol;


  _l1c_nesta_Rty(NP, NP->b, NP->Atb);
  cblas_dcopy(NP->m, NP->Atb, 1, NP->xo, 1);

  double *Ux_ref = NP->dwork1;

  _l1c_nesta_Wx(NP, NP->xo, Ux_ref);

  l1c_abs_vec(NP->m, Ux_ref, Ux_ref);
  double mu0 = 0.9 * l1c_max_vec(NP->m, Ux_ref);
  double tol0 = 0.1;

  /* We need mu_final and beta such that beta^n_cont * mu0= mu_final
     i.e., (beta^n_cont) = (mu_final/mu0)
     i.e., log(beta) = log(mu_final/mu0) / n_cont
     ie.,  beta = exp(log(mu_final/mu0) / n_cont)
   */

  *beta_mu = exp(log((mu_final / mu0)) / (double)n_continue);
  *beta_tol = exp(log((tol_final / tol0)) / (double)n_continue);

  /* After n continuation steps, NP->mu will again be what we said.*/
  NP->mu_j = mu0;
  NP->tol_j = tol0;

  return L1C_SUCCESS;

}


int l1c_nesta(l1c_int m, double *xk, double mu, l1c_int n, double *b,
              l1c_AxFuns ax_funs, double sigma){

  /* Later, make these options*/
  int max_inner_iter = 10000;
  int n_continuation = 5;

  int status=0; //, idx_fmu=0;


  double alpha_k=0, tau_k = 0;

  /*Lipschitz constant divided by mu */
  double L = 1;
  double tol = 1e-4;
  double fbar=0, rel_delta_fmu;
  double beta_mu, beta_tol;
  unsigned flags = L1C_SYNTHESIS;


  l1c_NestaProb *NP = _l1c_NestaProb_new(n, m);

  if (!NP ){
    return L1C_OUT_OF_MEMORY;
  }


  /* Initialize*/
  status += l1c_nesta_setup(NP,  &beta_mu, &beta_tol, n_continuation,
                            b, ax_funs, sigma, mu, tol, L, flags);
  if (status){
    return status;
  }

  struct l1c_fmean_fifo fbar_fifo = _l1c_new_fmean_fifo();

  cblas_dcopy(NP->m, NP->xo, 1, NP->xk, 1);

  for (int iter=1; iter<= n_continuation; iter++){

    /* Reset everthing.*/
    l1c_init_vec(L1C_NESTA_NMEAN, fbar_fifo.f_vals, 0);
    fbar_fifo.next = fbar_fifo.f_vals;
    fbar_fifo.n_total = 0;

    l1c_init_vec(m,  NP->gradf_sum, 0.0);

    NP->L_by_mu = L/NP->mu_j;
    printf("Starting nesta continuation iter %d, with muj = %f\n", iter, NP->mu_j);
    /* ---------------------------- MAIN ITERATION -------------------------- */
    printf("Iter |     fmu     |  Rel. Vartn fmu |  Residual  |\n");
    printf("------------------------------------------------------\n");
    for (int k=0; k < max_inner_iter; k++){
        l1c_nesta_feval(NP);

      /* ----------------- Update yk ------------------------- */
      l1c_nesta_project(NP, NP->xk, NP->gradf, NP->yk);

      /* From paragraph right before (2.3). Reference [43,50] proves convergence
         for these alpha_k, tau_k
      */
      alpha_k = 0.5 * (double)(k + 1);
      tau_k = 2.0 / ((double)k + 3.0);

      /* Update cummulative, weighted sum of gradients (3.8)-(3.12).*/
      cblas_daxpy(m, alpha_k, NP->gradf, 1, NP->gradf_sum, 1);

      /* Projection for zk */
      l1c_nesta_project(NP, NP->xo, NP->gradf_sum, NP->zk);

      /* ----------------- Update xk ----------------*/
      /* xk = tau_k * zk + (1-tau_k) * yk*/
      l1c_daxpby_z(m, tau_k, NP->zk, (1-tau_k), NP->yk, NP->xk);

      /*------------ Check for exit -----------------*/
      fbar = _l1c_mean_fmean_fifo(&fbar_fifo);
      rel_delta_fmu = (NP->fx - fbar) / fbar;

      _l1c_push_fmeans_fifo(&fbar_fifo, NP->fx);

      if (rel_delta_fmu < NP->tol){
        break;
      }
    } /* Inner iter*/

    NP->mu_j = NP->mu_j * beta_mu;
    NP->tol_j = NP->tol_j * beta_tol;
    cblas_dcopy(NP->m, NP->xk, 1, NP->xo, 1);
  }

  cblas_dcopy(m, NP->xk, 1, xk, 1);
  return status;
}
