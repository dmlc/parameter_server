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
- compiler: gcc >= 4.7 (tested on 4.7.x, 4.8.x) or clang >= 3.4 (tested
on 3.4). You can change the first line of [src/Makefile](src/Makefile) to switch
between gcc and clang.
- system: should work on both Linux and Mac OS (tested on Ubuntu 12.10, 13.10, Max OS X 10.8 10.9)
- depended libraries: The system depends on
  [zeromq](http://zeromq.org/),
  [gflags](https://code.google.com/p/gflags/),
  [glogs](https://code.google.com/p/google-glog/),
  [gtest](https://code.google.com/p/googletest/),
  [protobuf](https://code.google.com/p/protobuf/),
  [zlib](),
  [snappy](),
  [eigen3]() and optional [mpi](). We provide a way to build them from
  sources. First go to the parameter server directory, then
  ```
  git clone git@github.com:mli/parameter_server_third_party.git third_party
  cd third_party
  ./install.sh
  ```

Build parameter server.
> cd src && make -j8
The `-j` option specifies how many threads are used to
build the projects, you may change it to a more proper value.

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
