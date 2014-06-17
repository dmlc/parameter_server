function [I,J,V] = load_spbin(name)

% if exist([name '.lab'], 'file')
%     Y = load_bin([name '.lbl'], 'int32');
% else
%     Y = [];
% end

I = load_bin([name '.offset'], 'uint64');
J = load_bin([name '.index'], 'uint64');
V = load_bin([name '.value'], 'double');
