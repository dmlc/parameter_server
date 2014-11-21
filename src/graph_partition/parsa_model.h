#pragma once
#include "parameter/kv_map.h"
namespace PS {
namespace GP {

class ParsaModel : public KVMap<Key, uint64> {
 public:
  void partitionV(int num_partitions) {
    std::vector<int> cost(num_partitions);
    for (const auto& e : data_) {
      if (!e.second) continue;
      int max_cost = 0;
      int best_k = -1;
      for (int k = 0; k < num_partitions; ++k) {
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
    for (int j = 0; j < num_partitions; ++j) v += cost[j];
    LL << v;
  }

 private:
  // std::vector<int16> partition_V_;
  std::unordered_map<Key, uint8> partition_V_;
};

} // namespace GP
} // namespace PS
