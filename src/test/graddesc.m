
load('rcv1');
%%

prec = @(y,py) nnz(y.*((py>0.5)*2-1)==1) / length(y);

% binary logistic regression
grad = @(y,x,w) x'*(-y./(1+exp(y.*(x*w))));

hessian = @(y,x,w) (x.^2)'*(exp(y.*(x*w))./(1+exp(y.*(x*w))).^2);


objv = @(y,x,w) sum(log(1+exp(-y.*(x*w))));
pred = @(x,w) 1./(1+exp((x*w)));

max_iter = 20;
eta = .8;

w = zeros(size(X,2), 1);
% w = rand(p,1);

obj = [];
acc = [];

% y1 = Y(1:10121);
% x1 = X(1:10121,:);
% y2 = Y(10122:end);
% x2 = X(10122:end,:);

nm = @(x,w) sum(abs(w(sum(abs(x))~=0)));
for s = 1 : max_iter
  % gg = grad(Y,X,w); gg(1:5)'

  % [sum(grad(y1,x1,w)), sum(grad(y2,x2,w))]
  % [nm(x1, w), nm(x2, w)]
  g = grad(Y, X, w);
  % sum(abs(g));
  % g(1:10)'
  % w(1:10)'
  h = hessian(Y,X,w);

  % d = g ./ max(h, 1e-10);
  % w = w - eta * min(5, max(-5, d));
  w = w - eta * g;

  % [objv(y1,x1,w), objv(y2,x2,w)]
  % sum(g)
  % g(1:5)'
  % w(1:5)'
  obj(s) = objv(Y,X,w);
  acc(s) = prec(Y,1-pred(X,w));
  fprintf(sprintf('iter: %d, obj: %d, train acc: %.3f, |w|_1 %f\n', ...
                  s, obj(s), acc(s), sum(abs(w))));
end
