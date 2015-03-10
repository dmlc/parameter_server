#pragma once
namespace PS {

// the base class of a workload pool
class WorkloadPool {
 public:
  WorkloadPool() { }
  WorkloadPool(const Workload& load) { set(load); }
  virtual ~WorkloadPool() { }

  // set all workload
  void set(const Workload& load) { }

  // assign a piece of *workload* to *node_id*. return true
  bool assign(const NodeID& node_id, Workload* load) { return true; }

  // restored all unfinished workloads assigned to *node*
  void restore(const NodeID& node_id) { }

  // mark the workload with *id* as finished
  void finish(int id) { }
  // block until all workloads are finished
  void waitUtilDone() { }

 protected:
  Workload load_;
};

} // namespace PS
