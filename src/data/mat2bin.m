function mat2bin(name, Y, X)

[a,b,x] = find(X);

save_bin([name '.colidx'], a-1, 'uint64');
save_bin([name '.value'], x, 'double');

if nnz(x==0) + nnz(x==1) == length(x)
  E = X;
else
  E = sparse(a,b,1);
end
clear b a x

save_bin([name '.rowcnt'], cumsum([0 full(sum(E))]), 'uint64');
save_bin([name '.label'], Y, 'double');

fid = fopen([name, '.size'], 'w');
fprintf(fid, '0 %lu\n0 %lu\n%lu\n', size(X,2), size(X,1), nnz(X));
end
