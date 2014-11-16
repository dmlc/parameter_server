#pragma once
#include "parameter/kv_buffered_vector.h"

namespace PS {
namespace LM {

class BatchServer {
 public:
  void init(const string& name, const Config& conf);
  // TODO
  void updateWeight(const Call& cmd) { }
  void saveModel(const DataConfig& output);
 protected:
  KVBufferedVectorPtr<Key, double> model_;
  Config conf_;
};





} // namespace LM
} // namespace PS
