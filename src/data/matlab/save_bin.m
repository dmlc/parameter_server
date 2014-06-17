function save_bin(name, X, format)
%save a vector into a binary file
%save_bin(name, X, format)

if nargin < 3
    format = 'double';
end

fid = fopen(name, 'w');
n = fwrite(fid, X, format);
fclose(fid);

if n ~= numel(X)
    disp(sprintf('only write %d element, the actual one is %d', n, numel(X)));
end
