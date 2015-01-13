Example configurations to run applications on data RCV1.

Use `data/rcv1_small.sh` or `data/rcv1_large.sh` to prepare the dataset.

Script `script/local.sh` can be used to start multiple processes in local
machine.

For example, run l1-regularized logistic regression using block
coordinate descent with 1 server and 4 workers:

> script/local.sh 1 4 ../config/rcv1/batch_l1lr.conf

Test the trained model:

> script/local.sh 0 0 ../config/rcv1/eval_batch.conf

Or run l1-regularized logistic regression using online gradient descent with 1 server and 4 workers:

> script/local.sh 1 4 ../config/rcv1/online_l1lr.conf

and then test the model:

> script/local.sh 0 0 ../config/rcv1/eval_online.conf
