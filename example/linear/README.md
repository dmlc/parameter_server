# Tutorial to run linear method

**Prepearing Data**

Use the script such as `rcv1/download.sh` and `ctr/download.sh` to prepare
sample data.


**Run L1 Logistic Regression on the CTR dataset**

Let's first start one worker and one server in the local machine to run the online
solver.
```bash
../../script/ps.sh start ../../build/linear -app_file ctr/online_l1lr.conf
```

Next we use 3 workers and 4 servers:

```bash
../../script/ps.sh start -nw 3 -ns 4 ../../build/linear -app_file ctr/online_l1lr.conf
```

We can also start the jobs in multiple machines. Assume there is a `your_hostfile`
containing all machines IPs line by line. Then start the job via
the `-hostfile` option.
```bash
../../script/ps.sh start -hostfile your_hostfile -nw 3 -ns 4 ../../build/linear -app_file ctr/online_l1lr.conf
```

Finally, we can change the application by the `-app_file` option. For example, Evaluate the
trained model
```bash
../../script/ps.sh start ../../build/linear -app_file ctr/eval_online.conf
```
or train by the batch solver
```bash
../../script/ps.sh start ../../build/linear -app_file ctr/batch_l1lr.conf
```

See more information about the configuration file in [linear.proto](../../src/app/linear_method/proto/linear.proto)

<!-- *by [[www.docker.com][docker]]* -->
<!-- #+BEGIN_SRC bash -->
<!-- sudo ../../docker/local.sh 2 2 ctr/batch_l1lr.conf data model -->
<!-- #+END_SRC -->
