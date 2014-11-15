#pragma once
#include "linear_method/linear_method.h"
#include "data/slot_reader.h"
#include "parameter/kv_vector.h"
namespace PS {
namespace LM {

class BatchSolverWorker {
 public:
  void init(const string& name);

  void preprocessData(int time, const Call& cmd);
  int loadData(ExampleInfo* info);

  bool loadCache(const string& name) { return dataCache(name, true); }
  bool saveCache(const string& name) { return dataCache(name, false); }
  bool dataCache(const string& name, bool load);
 protected:
  // weight
  KVVector<Key, double> model_;

  // training data reader
  SlotReader slot_reader_;

  // training data
  std::map<int, MatrixPtr<double>> X_;
  MatrixPtr<double> y_;
  // dual_ = X * w
  SArray<double> dual_;

  //
  BatchSolver* solver_;
  std::mutex mu_;
};

} // namespace LM
} // namespace PS
