# Parameter Server

The parameter server is a distributed machine learning framework scaling to
industrial-level problems. It is a joint project by
[CMU SML-Lab](http://sml-lab.com), [Baidu IDL](http://idl.baidu.com/en/), and [Google](http://research.google.com).

## How to install

### Requirements
- Compiler: gcc >= 4.7.2 (prefer >=4.9) or llvm >= 3.4
- OS: tested on Ubuntu 12.10, 13.10, 14.04, RHEL 4U3, Mac OS X 10.9
- Dependent libraries: [zeromq](http://zeromq.org/),
  [gflags](https://code.google.com/p/gflags/),
  [glogs](https://code.google.com/p/google-glog/),
  [gtest](https://code.google.com/p/googletest/),
  [protobuf](https://code.google.com/p/protobuf/), [zlib](), [snappy](),
  [eigen3](). You can use
  [install.sh](https://github.com/mli/parameter_server_third_party) to build
  them from sources.

### Build the system
```
git clone https://github.com/mli/parameter_server
cd parameter_server
git clone https://github.com/mli/third_party
cd third_party && ./install.sh
cd .. && make -j8
```

Options for building:

- depended libraries are available in somewhere
```
export THIRD=/usr/local && make -j8
```

- statical linking
```
export STATIC=1 && make -j8
```

<!-- ## Input Data -->

<!-- The parameter system can read data in either raw binary format or protobuf -->
<!-- format. There is a `text2proto` program converting data from a range of text -->
<!-- formats into binary ones. See `data/rcv1_bianry.sh` for an example. -->

<!-- ### parameter server format -->

<!-- The format of one instance: -->

<!-- ``` -->
<!-- label;group_id feature[:value] feature[:value] ...;groud_id ...;...; -->
<!-- ``` -->

<!-- - *label*: +1/-1 for binary classification, 0,1,2,... for multiclass -->
<!-- classification, a float value for regression. And certainly it can be empty. -->
<!-- - *group_id*: an integer identity of a feature group, each instance should -->
<!-- contains at least one feature group. -->
<!-- - *feature*: for sparse data, it is an 64-bit integer presenting the feature id, -->
<!-- while for dense data, it is a float feature value -->
<!-- - *weight*: only valid for non-binary sparse data, it is a float feature -->
<!-- value. -->

<!-- ### [libsvm format](http://www.csie.ntu.edu.tw/~cjlin/libsvm/) -->

<!-- Sparse format: -->

<!-- ``` -->
<!-- label feature_id:value feature_id:value ... -->
<!-- ``` -->

<!-- <\!-- ### adfea -\-> -->
<!-- <\!-- Sparse binary format: -\-> -->

<!-- <\!-- ``` -\-> -->
<!-- <\!-- label feature_id:groud_id feature_id:grou_id ... -\-> -->
<!-- <\!-- ``` -\-> -->

<!-- ### [vowpal wabbit format](https://github.com/JohnLangford/vowpal_wabbit/wiki/Input-format) -->

<!-- TODO -->

## How to run
- Use `data/rcv1_binary.sh` to download an example dataset
- Run on the local machine: `script/local.sh`
- Start by `mpirun`: `script/mpi_root.sh config/rcv1_mpi.conf`
- Start by a cluster resource manager: In progress.
