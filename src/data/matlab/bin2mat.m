function [Y,X] = bin2mat(name);

[Y,i,j] = load_spbin(name);
n = length(Y);

assert(n==length(i)-1, 'length dont match');
J = double(j) + 1;
s = length(J);
p = max(J);
I = zeros(s,1);
for k = 1 : n
    I(i(k)+1:i(k+1)) = k;
end
clear i j

X = sparse(I,J,1);
