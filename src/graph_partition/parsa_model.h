#pragma once
#include "parameter/kv_map.h"
namespace PS {
namespace GP {

class ParsaModel : public KVMap<Key, uint64> {
 public:
  void init(const ParsaConfig& conf) { conf_ = conf; }

  virtual void setValue(const MessagePtr& msg) {
    // clear the neigbhor set in the initialization stage
    ++ count_;
    int stage1_real_work = conf_.stage1_warm_up_blocks() +
                           conf_.stage0_warm_up_blocks() +
                           conf_.stage0_blocks();
    int chn = msg->task.key_channel();
    if (chn == stage1_real_work && !enter_real_stage1_) {
      enter_real_stage1_ = true;
      // data_.clear();
    } else if (chn < stage1_real_work &&
               count_ % conf_.clear_nbset_per_num_blocks() == 0) {
      // data_.clear();
    }

    KVMap<Key, uint64>::setValue(msg);
  }

  void partitionV(int num_partitions, bool random_partition) {
    std::vector<int> cost(num_partitions);
    srand(time(NULL));
    for (const auto& e : data_) {
      if (!e.second) continue;
      if (random_partition) {
        int best_k = rand() % num_partitions;
        for (int k = 0; k < num_partitions; ++k) {
          if ((e.second & (1 << k)) && k != best_k) {
            ++ cost[k];
          }
        }
      } else {
        // greedy
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
    }

    int v = 0;
    for (int j = 0; j < num_partitions; ++j) v += cost[j];
    LL << v;
  }

 private:
  bool enter_real_stage1_ = false;
  int count_ = 0;
  ParsaConfig conf_;
  std::unordered_map<Key, uint8> partition_V_;
};

} // namespace GP
} // namespace PS
