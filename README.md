<img src="http://parameterserver.org/images/parameterserver.png" alt="Parameter Server" style="width: 500px;">

The parameter server is a distributed system scaling to industry size machine
learning problems. It provides asynchronous and zero-copy key-value pair
communications between worker machines and server machines. It also supports
flexiable data consistency model, data filters, and flexiable server machine
programming.

NOTE: We are refactoring this repository in the lite branch, there is a simpler
communication interface, and application codes will be moved to other DMLC
repositories soon.

- [Document](doc/)
- [Wiki](https://github.com/dmlc/parameter_server/wiki/)
- How to [build](make/)
- Examples
  - [Linear method](example/linear) [Linear method with Cloud](docker)
  - Deep neural network, see [CXXNET](https://github.com/dmlc/cxxnet) and [Minverva](https://github.com/minerva-developers/minerva)
