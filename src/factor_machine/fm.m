%% load data
n = floor(size(X,1) * .7);
tr_X = X(1:n, :)';
tr_Y = Y(1:n, :);

ts_X = X(n+1:end, :);
ts_Y = Y(n+1:end);

% n = size(X,1);
% tr_X = X';
% tr_Y = Y;
%% config

% embeding size
k = 5;
% minibatch size
m = 10000;
% number of data pass
pass = 10;
% std of initialization
sigma = .01;
% learning rate
eta_w = .1;
eta_v = .2;
% l2 regularizor
lambda_w = 1;
lambda_v = 10;

% init
p = size(X,2);
w = randn(p,1) * sigma;
v = randn(p,k) * sigma;
sum_gw = ones(p,1) * 1e-20;
sum_gv = ones(p,k) * 1e-20;
% iterate
for ip = 1 : pass
  itv = 1 : m : n+1;
  rdp = randperm(length(itv)-1);
  for i = 1 : length(itv)-1
% slice a minibatch
    ix = itv(rdp(i)) : itv(rdp(i)+1)-1;
    x = tr_X(:,ix)';
    y = tr_Y(ix);
% gradient
    j = find(sum(x));
    xj = x(:,j);
    vj = v(j,:);
    wj = w(j);
    py = xj * wj + .5 * sum((xj*vj).^2 + (xj.*xj)*(vj.*vj), 2);
    if mod(i, 10) == 0
      fprintf('%d\t%d\t%d\n', i, auc(y, py), mean(log(1+exp(-y .* py))))
    end

    p = - y ./ (1 + exp (y .* py));
    gw = xj' * p + lambda_w * wj;
    gv = xj' * bsxfun(@times, p, xj*vj) - bsxfun(@times, (xj.*xj)'*p, vj) ...
         + lambda_v * vj;
% ada grad
    sum_gw(j) = sum_gw(j) + gw.^2;
    sum_gv(j,:) = sum_gv(j,:) + gv.^2;

    w(j) = wj - eta_w * gw ./ sqrt(sum_gw(j));
    v(j,:) = vj - eta_v * gv ./ sqrt(sum_gv(j,:));
  end
end

% test
py = ts_X * w + .5 * sum((ts_X*v).^2 + (ts_X.*ts_X)*(v.*v), 2);
auc(ts_Y, py)
