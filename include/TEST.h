#ifndef _TEST_
#define _TEST

Suite * l1qc_newton_suite(void);

Suite * cgsolve_suite(void);

Suite * dct_suite(void);

#if !defined(_USEMKL_)
Suite * dct2_suite(void);
#endif

Suite * dct_mkl_suite(void);

Suite *vcl_math_suite(void);

Suite *bregman_suite(void);

Suite *TV_suite(void);

Suite *l1c_math_suite(void);

Suite *l1c_memory_suite(void);

Suite *matrix_transform_suite(void);
#endif
