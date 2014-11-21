#pragma once
#include "graph_partition/graph_partition.h"
namespace PS {
namespace GP {

class ParsaScheduler : public GraphPartition {
 public:
  virtual void init();
  virtual void run();
 private:
};

} // namespace GP
} // namespace PS
