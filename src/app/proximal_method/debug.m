n = size(X,1);
exw = ones(n,1);

ix = 1;
x = X(:,ix);
x = bsxfun(@times, Y, x);
g = -x'*tau;
u = full(sum(min(bsxfun(@times, tau.*(1-tau), z), .25) .* x .* x)');


gp = g + 1;
gn = g - 1;

p = length(ix);
wj = zeros(p,1);
d = -wj;
ix = gp <= u .* wj;
d(ix) = - gp(ix) ./ u(ix);
ix = gn >= u .* wj;
d(ix) = - gn(ix) ./ u(ix);

d = min(1, max(-1, d));

