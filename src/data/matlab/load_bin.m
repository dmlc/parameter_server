function data = load_bin(filename, format, offset, length)
%load a vector from a binary file
%load_bin(filename, format, offset, length)

if nargin < 2, format = 'double'; end
if nargin < 3, offset = 0; end
if nargin < 4, length = inf; end

fid=fopen(filename,'r');
if (fid < 0)
  data = [];
    disp([filename ' doesn''t exist']);
    return;
end

eval(['v=', format, '(1);']);
w = whos('v');
bsize = w.bytes;

fseek(fid, bsize*offset, -1);
data = fread(fid, length, [format,'=>',format]);
fclose(fid);

end
