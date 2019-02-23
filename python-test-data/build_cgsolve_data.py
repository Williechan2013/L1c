import numpy as np
import json
import codecs

from numpy.random import seed
def build_cgsolve_test_data(test_data_root):
    # Build and save test data for cgsolve.c
    #
    # The JSON file will be saved to test_data_root/''cgsolve_small01.json'
    #

    cg_test_data_small_path = 'cgsolve_small01.json'
    tol = 1e-9
    maxiter = 500

    # A slightly larger example.
    seed(2)
    N = 50
    A0 = np.random.rand(N, N)*50
    A = A0.dot(A0.T) + 1*np.eye(N)

    # NOTE: errors between matlab cgsolve and c cgsolve for this small problem
    # seem to come from errors between the two matrix-vector multiplications. I
    # tested this by stopping each solver in the first iteration immediatly
    # after
    # the q = A*d step and printing out d and q to 16 decimal places (in the c
    # code). Copying and pasting that into matlab, I get that d_c - d_matlab = 0
    # everywhere, but q_c - q_matlab has an error of about 1e-13. I can only
    # guess that this is because the two dgemv function must do things in
    # slightly different order. I don't really know, but that's not what I'm
    # trying to test here. So, Im going to multiply the random data up and round
    # to zero decimal places. The resulting vector solution are now accurate to
    # within

    assert(np.min((np.linalg.eigvals(A))) > 0)
    b = np.random.rand(N)*50
    # b = np.round(b, 0)

    # A_fun = @(x) A_mat *x
    # x = L1qcTestData.cgsolve(A_fun, b, tol, maxiter, verbose)
    x = np.linalg.solve(A, b)
    # A_row = A.flatten()
    A_row = np.zeros(0)

    # we use the upper triangle only. Numpy probably has something for that,
    # but I dont know it.
    for i in range(0, A.shape[0]):
        A_row = np.hstack((A_row, A[i, i:N]))

    data = {'A': A_row.tolist(),
            'b': b.tolist(),
            'x': x.tolist(),
            'tol': tol,
            'max_iter': maxiter}

    json.dump(data, codecs.open(cg_test_data_small_path, 'w', encoding='utf-8'),
              separators=(',', ':'), sort_keys=True, indent=4)


def ax_sym_data(test_data_root):
    ax_sym_path = 'ax_sym.json'
    # ------------------------------------------------ #
    seed(1)
    N = 50
    A = np.random.rand(N, N)

    A = A.dot(A.T) + 6*np.eye(N)

    assert(np.min(np.linalg.eigvals(A)) > 0)


    x = np.random.rand(N, 1)

    y = A.dot(x)
    # we use the upper triangle only. Numpy probably has something for that,
    # but I dont know it.
    A_row = np.zeros(0)
    for i in range(0, A.shape[0]):
        A_row = np.hstack((A_row, A[i, i:N]))

    data = {'A': A_row.tolist(),
            'y': y.tolist(),
            'x': x.tolist()}

    json.dump(data, codecs.open(ax_sym_path, 'w', encoding='utf-8'),
              separators=(',', ':'), sort_keys=True, indent=4)

    # Arow = []
    # for i=1:size(A, 1)
    # Arow = [Arow, A(i,i:end)]
    # end

    # jopts.FileName = 'test_data/ax_sym.json'
    # jopts.FloatFormat = '%.20f'

    # savejson('', struct('A', Arow(:)', 'x', x(:)', 'y', y(:)'), jopts)


build_cgsolve_test_data('')

ax_sym_data('')
