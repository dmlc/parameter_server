#pragma once
#include "parameter/kv_buffered_vector.h"
namespace PS {
namespace LM {

class BatchCommon {
 protected:
  // the global shared model
  KVBufferedVectorPtr<Key, double> model_;

  // all feature group ids
  std::vector<int> fea_grp_;
  const int k_time_ratio_ = 10;
};

} // namespace LM

} // namespace PS
