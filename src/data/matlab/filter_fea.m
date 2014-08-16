% Y = bin2mat('CTRb.Y');
% [X, group_id, group_os] = bin2mat('CTRb.X');

pv = 4;

ix = sum(X) > pv;
X = X(:,ix);

os = group_os;
for i = 2 : length(os)
  os(i) = os(i-1) + nnz(ix(group_os(i-1):group_os(i)-1));
end
group_os = os;


% save CTRb_pv4 X Y group_id group_os
