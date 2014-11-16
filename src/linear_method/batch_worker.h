#pragma once
#include "linear_method/computation_node.h"
#include "data/slot_reader.h"
#include "parameter/kv_vector.h"
namespace PS {
namespace LM {

class BatchSolverWorker : public CompNode, public BatchCommon {
 public:
  virtual void init();
  virtual void loadData(ExampleInfo* info, int *hit_cache);
  virtual void preprocessData(const MessagePtr& msg);
  virtual void iterate(const MessagePtr& msg) { computeGradient(msg); }
  virtual void evaluateProgress(Progress* prog) { }
 protected:
  // TODO
  void computeGradient(const MessagePtr& msg) { }
  bool loadCache(const string& name) { return dataCache(name, true); }
  bool saveCache(const string& name) { return dataCache(name, false); }
  // TODO
  bool dataCache(const string& name, bool load);

  // training data reader
  SlotReader slot_reader_;

  // training data
  std::map<int, MatrixPtr<double>> X_;
  MatrixPtr<double> y_;
  // dual_ = X * w
  SArray<double> dual_;

  // the timestamps when the model will be initialized on the servers
  std::unordered_map<int, int> model_ready_;
};

} // namespace LM
} // namespace PS
