#pragma once
#include "util/common.h"
#include "system/message.h"
#include "learner/proto/workload.pb.h"
namespace PS {

// the base class of a workload pool. thread safe

class WorkloadPool {
 public:
  WorkloadPool() { }
  WorkloadPool(const Workload& load) { set(load); }
  virtual ~WorkloadPool() { }

  // set all workload
  void set(const Workload& load);

  // assign a piece of *workload* to *node_id*. return true
  bool assign(const NodeID& node_id, Workload* load);

  // restored all unfinished workloads assigned to *node*
  void restore(const NodeID& node_id);

  // mark the workload with *id* as finished
  void finish(int id);
  // block until all workloads are finished
  void waitUtilDone();

 protected:
  struct WorkloadInfo {
    Workload load;
    NodeID node;
    bool assigned = false;
    bool finished = false;
  };
  std::vector<WorkloadInfo> loads_;
  int num_finished_ = 0;
  std::mutex mu_;
};

} // namespace PS
