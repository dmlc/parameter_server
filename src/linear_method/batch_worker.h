#pragma once
#include "linear_method/linear_method.h"
#include "data/slot_reader.h"
#include "parameter/kv_vector.h"
namespace PS {
namespace LM {

class BatchSolverWorker {
 public:
  void init(const string& name, const Config& conf, BatchSolver* solver);
  int loadData(ExampleInfo* info);
  void preprocessData(int time, const Call& cmd);
  void computeGradient(int time, const MessagePtr& msg);
 protected:
  bool loadCache(const string& name) { return dataCache(name, true); }
  bool saveCache(const string& name) { return dataCache(name, false); }
  bool dataCache(const string& name, bool load);

  // weight
  KVVector<Key, double> model_;

  // training data reader
  SlotReader slot_reader_;

  // training data
  std::map<int, MatrixPtr<double>> X_;
  MatrixPtr<double> y_;
  // dual_ = X * w
  SArray<double> dual_;

  BatchSolver* solver_;
  Config conf_;
  // std::mutex mu_;
};

} // namespace LM
} // namespace PS
