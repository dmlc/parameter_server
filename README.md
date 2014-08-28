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
  [eigen3](). We provide
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
format. There is a `text2proto` program converting data from a range of text
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

<!-- ### adfea -->
<!-- Sparse binary format: -->

<!-- ``` -->
<!-- label feature_id:groud_id feature_id:grou_id ... -->
<!-- ``` -->

### [vowpal wabbit format](https://github.com/JohnLangford/vowpal_wabbit/wiki/Input-format)

TODO

## Run

### On local machine

See `script/local.sh`

### Start by `mpirun`

Run `script/mpi_root.sh` at the root machine. An example configuration is in `config/mpi.conf`.

### Start by `yarn`

In progress.
