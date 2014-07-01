# Parameter Server

Parameter server is a distributed machine learning framework. It scales machine
learning applications to industry-level problems, namely 10 to 100 billions of
samples and features, 100 to 1000 machines. It is a joint project by
[CMU SML-Lab](http://sml-lab.com), [Baidu IDL](http://idl.baidu.com/en/), and [Google](http://research.google.com).

## Features

- **Efficient communication.** All communications are asynchronous. It is
  optimized for machine learning tasks to reduce network traffic and overhead.
- **Flexible consistency models.** The system provides flexible consistency
  models to allow the algorithm designer to balance algorithmic convergence rate
  and system efficiency, where the best trade-off depends on data, algorithm,
  and hardware.
- **Elastic Scalability.** New nodes can be added without restarting the running
  framework.
- **Fault Tolerance and Durability.** Recovery from and repair of
  non-catastraphic machine failures within several seconds, without interrupting
  computation.
- **Ease of Use.** The globally shared parameters are represented as
  (potentially sparse) vectors and matrices to facilitate development of machine
  learning applications. The linear algebra data types come with
  high-performance multi-threaded linear algebra libraries.

## How to Build

Requirement:
- compiler: gcc >= 4.8 (prefer 4.9 with better support for <regex>) or llvm >= 3.4 (tested
on Apple LLVM version 5.1).
- system: Should work on both Linux and Mac OS (tested on Ubuntu 12.10, 13.10,
  RHEL 4U3, Max OS X 10.9)
- depended libraries: [zeromq](http://zeromq.org/),
  [gflags](https://code.google.com/p/gflags/),
  [glogs](https://code.google.com/p/google-glog/),
  [gtest](https://code.google.com/p/googletest/),
  [protobuf](https://code.google.com/p/protobuf/), [zlib](), [snappy](),
  [eigen3]() and optional [mpi](). We provide
  [install.sh](https://github.com/mli/parameter_server_third_party) to build
  them from sources automatically.

Build parameter server:

```
cd src && make -j8
```

The `-j` option specifies how many threads are used to
build the projects, you may change it to a more proper value.

If you want to static link all library, use:                                                                                     
```                                                                                                                               
make -j8 args=static                                                                                                              
```

## Input Data

The system can read both raw binary data and protobuf data. It
also supports several text formats. In a text format, each example is
presented as a line of plain text.

### parameter server format

The format of one instance:

```
label;group_id feature[:value] feature[:value] ...;groud_id ...;...;
```

- *label*: +1/-1 for binary classification, 0,1,2,... for multiclass
classification, a float value for regression. And certainly it can be empty.
- *group_id*: an integer identity of a feature group, each instance should
contains at least one feature group.
- *feature*: for sparse data, it is an 64-bit integer presenting the feature id,
while for dense data, it is a float feature value
- *weight*: only valid for non-binary sparse data, it is a float feature
value.

### More in progress

The [libsvm format](http://www.csie.ntu.edu.tw/~cjlin/libsvm/) uses the sparse
format. It presents each instance as:

```
label feature_id:value feature_id:value ...
```

[vowpal wabbit format](https://github.com/JohnLangford/vowpal_wabbit/wiki/Input-format)

### binary format

Check examples on [RCV1](https://github.com/mli/parameter_server_data).

## How to start

One way to start the system is using `mpirun`. To install `mpi` on Ubuntu, run `sudo
apt-get install mpich`, or `sudo port install mpich` for Mac OS. The system can be
also started via `ssh` and resource manager such as `yarn` without mpi (In progress).

```
mpirun -np 4 ./ps_mpi -num_servers 1 -num_workers 2 -app ../config/rcv1_l1lr.config
```

The augments:
- -np: the number of processes created by `mpirun`. It should >= num_workers +
num_servers + 1 (the scheduler). Use `-hostfile hosts` to specify the machines.
- -interface: the network interface, run `ifconfig` to find the available
  network interfaces
- -num_workers: the number of worker nodes. Each one will get a part of training
  data
- -num_servers: the number of server nodes. Each one will get a part of the
  model.
- -app: the application configuration

Use `./ps_mpi --help` to see more arguments.

## Wrap up

To run sparse logistic regression on RCV1 from scratch:

```
mkdir your_working_dir
cd your_working_dir
git clone git@github.com:mli/parameter_server.git .
git clone git@github.com:mli/parameter_server_third_party.git third_party
git clone git@github.com:mli/parameter_server_data.git data
third_party/install.sh
cd src && make -j8
mpirun -np 4 ./ps_mpi -num_servers 1 -num_workers 2 -app ../config/rcv1_l1lr.config
```
