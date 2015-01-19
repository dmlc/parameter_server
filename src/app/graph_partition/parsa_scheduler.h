#pragma once
#include "graph_partition/graph_partition.h"
namespace PS {
namespace GP {

class ParsaScheduler : public GraphPartition {
 public:
  virtual void init();
  virtual void run();
  virtual void process(const MessagePtr& msg) { }
 private:
};

} // namespace GP
} // namespace PS
