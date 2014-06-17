function mat2bin(name, Y, X)
% new version
% save to row-majored binary format
% if nargin < 4, index_fmt = 'uint32'; end

[a,b,x] = find(X');

save_bin([name '.index'], uint32(a-1), 'uint32');

bool = 'false';
if nnz(x==0) + nnz(x==1) == length(x)
  disp('binary data');
  bool = 'true';
  save_bin([name '.offset'], uint64(cumsum([0; full(sum(X,2))])), 'uint64');
else
  save_bin([name '.value'], x, 'double');
  clear x
  E = sparse(a,b,1);
  save_bin([name '.offset'], uint64(cumsum([0 full(sum(E))])), 'uint64');
end

save_bin([name '.label'], Y, 'double');

fid = fopen([name, '.info'], 'w');


fprintf(fid, strcat('row_major    : true\n',...
         'sparse       : true\n',...
         'bool_value   : %s\n',...
         'row_begin    : 0\n',...
         'row_end      : %lu\n',...
         'col_begin    : 0\n',...
         'col_end      : %lu\n',...
         'entries      : %lu\n',...
         'sizeof_index : 4\n',...
         'sizeof_value : 8\n'), bool, size(X,1), size(X,2), nnz(X));
fclose(fid);
% fprintf(fid, '0 %lu\n0 %lu\n%lu\n', size(X,1), size(X,2), nnz(X));

end
