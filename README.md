# Parameter Server

The parameter server is a distributed machine learning framework scaling to
industrial-level problems. It is a joint project by
[CMU SML-Lab](http://sml-lab.com), [Baidu IDL](http://idl.baidu.com/en/), and [Google](http://research.google.com).

## Install

### Requirements:
- compiler: gcc >= 4.7.2 or llvm >= 3.4 (known problems with gcc <= 4.7, but
  didn't test for llvm)
- system: should work on both Linux and Mac OS (tested on Ubuntu 12.10, 13.10,
  RHEL 4U3, Max OS X 10.9)
- dependent libraries: [zeromq](http://zeromq.org/),
  [gflags](https://code.google.com/p/gflags/),
  [glogs](https://code.google.com/p/google-glog/),
  [gtest](https://code.google.com/p/googletest/),
  [protobuf](https://code.google.com/p/protobuf/), [zlib](), [snappy](),
  [eigen3]() and optional [mpi](). We provide
  [install.sh](https://github.com/mli/parameter_server_third_party) to build
  them from sources automatically.

### Build
The following steps download sources codes and data, and then build the
dependent libraries and the parameter server:

```
git clone https://github.com/mli/parameter_server
cd parameter_server
git clone https://github.com/mli/third_party
cd third_party && ./install.sh
cd .. && make -j8
```

Several options are available for building:

- depended libraries are install somewhere
```
export THIRD=/usr/local && make -j8
```

- statically linking all libraries:
```
export STATIC=1 && make -j8
```

- failed to install google hash, so use =std::map= instead
```
export GOOGLE_HASH=0 && make -j8
```

## Input Data

The parameter system can read data in either raw binary format or protobuf
format. There is a `text2bin` program converting data from a range of text
formats into binary ones. See `data/rcv1_bianry.sh` for an example.

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

### [libsvm format](http://www.csie.ntu.edu.tw/~cjlin/libsvm/)

Sparse format:

```
label feature_id:value feature_id:value ...
```

### adfea

Sparse binary format:

```
label feature_id:groud_id feature_id:grou_id ...
```

### [vowpal wabbit format](https://github.com/JohnLangford/vowpal_wabbit/wiki/Input-format)

TODO

## Run the parameter server

One way to start the system is using `mpirun`. The other ways such as starting
via `ssh` or via resource manager e.g. `yarn` are in progress.

An sample command to start the parameter server
```
mpirun -np 4 ./ps_mpi -num_servers 1 -num_workers 2 -num_threads 1 -interface lo0 -app ../config/rcv1_l1lr.config
```

The arguments:
- -np: the number of processes created by `mpirun`. It should >= num_workers +
num_servers + 1 (the scheduler). Use `-hostfile` to specify the machines.
- -interface: the network interface, run `ifconfig` to find the available
  network interfaces
- -num_workers: the number of worker nodes. Each one will get a part of training
  data
- -num_servers: the number of server nodes. Each one will get a part of the
  model.
- -num_threads: how many threads one work will use
- -app: the application configuration

Use `./ps_mpi --help` to see more options.
