name = 'smalldata';
n = 10;
p = 5;
data = round(100 * rand(10, 5) - 50);
label = round(rand(n, 1));
zcount = 30;
k = 0;
while k < zcount
    x = randi([1 n], 1);
    y = randi([1 p], 1);
    data(x, y) = 0;
    k = k + 1;
end

save 'data.mat' data
save 'y.mat' label
mat2bin(name, label, data');
