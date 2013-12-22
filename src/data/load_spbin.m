function [Y,I,J,V] = load_spbin(name)

if exist([name '.lbl'], 'file')
    Y = load_bin([name '.lbl'], 'int32');
else
    Y = [];
end

I = load_bin([name '.cnt'], 'uint64');
J = load_bin([name '.idx'], 'uint32');
V = load_bin([name '.val'], 'double');
