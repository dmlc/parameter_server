# Graph Partition

Implemented Parsa from the following paper

> Graph Partitioning via Parallel Submodular Approximation
> Mu Li, Dave Andersen, Alex Smola
> 2014

Some known issues:

1. This implementation handles general uint64 keys. A implementation which
   assume continous keys can be much faster.
2. One need to change the configuration in `parsa_common.h` to obtain more than
   16 partitions.
3. `partitionU` may be hung if assign too many blocks to one worker.
