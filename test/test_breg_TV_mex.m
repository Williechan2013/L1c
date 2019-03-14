
function [tm_mex, Xopt] = test_breg_TV_mex(fpath, verbose)
  dat=loadjson(fpath);

  n = sqrt(dat.N);
  m = n;
  x = dat.x_orig(:) + rand(dat.N,1);
  img = reshape(x, n, n);
  mu = 5;

  tic
  Xopt = breg_anistropic_TV(img, mu, 0.001, 1000);
  tm_mex = toc;
  
  fprintf('mex file time: %.4f\n', tm_mex);
  
  if size(Xopt, 1) == n && size(Xopt, 2) == m
    exit(0);
  else
    exit(1);
  end

end

