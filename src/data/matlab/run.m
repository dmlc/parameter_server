name = '../../data/rcv1';
load(name);

%% split into train and test
[n, p] = size(X);
X = X';
ix = randperm(n);
k = round(n*.6);

train_ix = ix(1:k);
test_ix = ix(k+1:end);

mat2bin([name, '.train'], Y(train_ix), X(:, train_ix))
mat2bin([name, '.test'], Y(test_ix), X(:, test_ix))
