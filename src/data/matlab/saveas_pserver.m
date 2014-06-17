function saveas_pserver(file_name, Y, X, group_id, binary)
% debug use only, not efficient for large data

if nargin < 4, group_id = zeros(size(X,2),1); end
if nargin < 5, binary = false;  end

assert(issorted(group_id));

tX = X';
fd = fopen(file_name, 'w');
for i = 1 : length(Y)
  fprintf(fd, '%d', Y(i));
  [a,b,c] = find(tX(:,i));
  pre_gid = -1;
  for j = 1 : length(a)
    id = group_id(a(j));
    if id ~= pre_gid
      fprintf(fd, '; %d', id);
      pre_gid = id;
    end

    if binary
      fprintf(fd, ' %d', a(j)-1);
    else
      fprintf(fd, ' %d:%g', a(j)-1, c(j));
    end
  end
  fprintf(fd, ';\n');
end
fclose(fd);
