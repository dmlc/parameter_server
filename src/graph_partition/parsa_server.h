#pragma once
#include "parameter/kv_map.h"
namespace PS {

class ParsaServer : public KVMap<Key, uint64> {
 public:
  void setConf(const ParsaConf& conf) {
    conf_ = conf;
    num_partitions_ = conf_.num_partitions();
  }

  void partitionV() {
    std::vector<int> cost(num_partitions_);
    for (const auto& e : data_) {
      if (!e.second) continue;
      int max_cost = 0;
      int best_k = -1;
      for (int k = 0; k < num_partitions_; ++k) {
        if (e.second & (1 << k)) {
          int c = ++ cost[k];
          if (c > max_cost) { max_cost = c; best_k = k; }
        }
      }
      CHECK_GE(best_k, 0);
      // partition_V_[e.first] = best_k;
      -- cost[best_k];
    }

    int v = 0;
    for (int j = 0; j < num_partitions_; ++j) v += cost[j];
    LL << v;
  }

 private:
  int num_partitions_;
  ParsaConf conf_;

  // std::vector<int16> partition_V_;
  std::unordered_map<Key, uint8> partition_V_;
};
} // namespace PS
