# Parameter Server

Parameter server is distributed machine learning framework. It scales machine
learning applications to industry level problems.

## Features

- **Efficient communication:** The asynchronous communication model does not
  block computation (unless requested). It is optimized for machine learning
  tasks to reduce network traffic and overhead.

- **Flexible consistency models:** Relaxed consistency further hides
  synchronization cost and latency. We allow the algorithm designer to balance
  algorithmic convergence rate and system efficiency. The best trade-off depends
  on data, algorithm, and hardware.

- **Elastic Scalability:** New nodes can be added without restarting the running
  framework.

- **Fault Tolerance and Durability:** Recovery from and repair of
  non-catastraphic machine failures within several seconds, without interrupting
  computation.  Vector clocks ensure well-defined behavior after network
  partition and failure.

- **Ease of Use:** The globally shared parameters are represented as
  (potentially sparse) vectors and matrices to facilitate development of machine
  learning applications. The linear algebra data types come with
  high-performance multi-threaded linear algebra libraries.

## Build

```
cd src
make -j8
```

- compiler: gcc >= 4.7 (tested on 4.7.x, 4.8.x) or clang >= 3.4 (tested
on 3.4)
- system: should works on both Linux and Mac OS (tested on Ubuntu 12.10, 13.10, Max OS X 10.8 10.9)
- depended libraries. An easy way to install
```
git clone git@github.com:mli/parameter_server_third_party.git third_party
cd third_party
./install.sh
```
or you can install the following manually
  - [Zeromq](http://zeromq.org/)
  - [gflags](https://code.google.com/p/gflags/)
  - [glogs](https://code.google.com/p/google-glog/)
  - [gtest](https://code.google.com/p/googletest/)
  - [protobuf](https://code.google.com/p/protobuf/)
  - [zlib]()
  - [snappy]()
  - [eigen3]()

## Input Data

Parameter server supports several data formats. An easy way is to use a text
format, and then use the provided program to convert them into binary format.

### text format

In a text format, each instance is presented as a line of plain text.  It is
easy to be read by human, but at the cost of reading and parsing. We provide
program to parse the following text formats into a protobuf or a binary format.

#### parameter server format

The format of one instance:

```
label;group_id feature[:value] feature[:value] ...;groud_id ...;...;
```

- *label*: the label, +1/-1 for binary classification, 0,1,2,... for multiclass
classification, a float value for regression. And certainly it can be empty.

- *group_id*: an integer identity of a feature group, each instance should
contains at least one feature group.

- *feature*: for sparse data, it is an 64-bit integer presenting the feature id,
while for dense data, it is a float feature value

- *weight*: only valid for non-binary sparse data, it is a float feature
value.

#### libsvm format

The [libsvm format](http://www.csie.ntu.edu.tw/~cjlin/libsvm/) uses the sparse
format. It presents each instance as:

```
label feature_id:value feature_id:value ...
```

#### vowpal wabbit format

[link](https://github.com/JohnLangford/vowpal_wabbit/wiki/Input-format)

TODO

### protobuf format

see [pserver_input.proto](src/proto/pserver_input.proto) for more details.

### bianry format

The binary format is the direct dump of the memory into the disk.

Each feature group, or the label are stored as a binary matrix.

A row-majored  stores the nonzero entries in each row
sequentially. For example, consider the following the matrix

```
[ 1 2 0 0 ]
[ 0 3 9 0 ]
[ 0 1 4 0 ]
```

Then it is stored by three binary files

```
name.offset = [ 0 2 4 6 ]      // array of offsets of first nonzero element of a row
name.index = [ 0 1 1 2 1 2 ]   // array of column index of each element
name.value  = [ 1 2 3 9 1 4 ]  // array of non-zero element value
```

## Run


```
git clone git@github.com:mli/parameter_server_data.git data
```
config, how to start, ...
