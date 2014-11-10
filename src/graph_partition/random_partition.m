%% load data

load rcv1

D = binarize(X);

%%

k = 4;
[n,p] = size(D);

Uix = randperm(n);
Useg = round(linspace(1,n+1,k+1));
neighbor = zeros(p,k);
for i = 1 : k
  neighbor(:,i) = full(sum(D(Useg(i):Useg(i+1)-1, :)))';
end

Vix = randperm(p);
Vseg = round(linspace(1,p+1,k+1));
cost = nnz(neighbor);
for i = 1 : k
  cost = cost - nnz(neighbor(Vix(Vseg(i):Vseg(i+1)-1),i));
end

cost
