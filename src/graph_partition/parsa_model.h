#pragma once
#include "parameter/kv_map.h"
#include "graph_partition/parsa_common.h"
namespace PS {
namespace GP {

class ParsaModel : public KVMap<Key, V> {
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
      data_.clear();
    } else if (chn < stage1_real_work &&
               count_ % conf_.clear_nbset_per_num_blocks() == 0) {
      data_.clear();
    }

    if (chn >= stage1_real_work) {
      // store the key for workers
      SArray<Key> recv_key(msg->key);
      worker_key_[msg->sender] = recv_key.setUnion(worker_key_[msg->sender]);
    }

    KVMap<Key, V>::setValue(msg);
  }

  void partitionV(int num_partitions, bool random_partition) {
    std::vector<int> cost(num_partitions);
    srand(time(NULL));

    int n = data_.size();
    SArray<KP> partition; partition.reserve(n);
    for (const auto& e : data_) {
      if (!e.second) continue;
      if (random_partition) {
        P best_k = rand() % num_partitions;
        for (int k = 0; k < num_partitions; ++k) {
          if ((e.second & (1 << k)) && k != best_k) {
            ++ cost[k];
          }
        }
      } else {
        // greedy
        int max_cost = 0;
        P best_k = -1;
        for (P k = 0; k < num_partitions; ++k) {
          if (e.second & (1 << k)) {
            int c = ++ cost[k];
            if (c > max_cost) { max_cost = c; best_k = k; }
          }
        }
        CHECK_GE(best_k, 0);
        partition.pushBack(std::make_pair(e.first, best_k));
        -- cost[best_k];
      }
    }

    int v = 0;
    for (int j = 0; j < num_partitions; ++j) v += cost[j];
    LL << data_.size() << " " << v;
    data_.clear();

    // push the results to workers
    std::sort(partition.begin(), partition.end(),
              [](const KP& a, const KP& b) { return a.first < b.first; });
    SArray<Key> V_key(n);
    SArray<V> V_val(n);
    for (int i = 0; i < n; ++i) {
      V_key[i] = partition[i].first;
      V_val[i] = partition[i].second;
    }
    partition.clear();

    int chn = conf_.stage0_warm_up_blocks() + conf_.stage0_blocks() +
              conf_.stage1_warm_up_blocks() + conf_.stage1_blocks();
    for (const auto& it : worker_key_) {
      MessagePtr V_msg(new Message(it.first, chn*3));
      V_msg->task.set_key_channel(chn);
      V_msg->setKey(it.second);
      SArray<V> val;
      parallelOrderedMatch(V_key, V_val, it.second, &val);
      V_msg->addValue(val);
      this->set(V_msg)->set_gather(true);
      this->push(V_msg);
    }
  }

 private:
  bool enter_real_stage1_ = false;
  int count_ = 0;
  ParsaConfig conf_;
  std::unordered_map<NodeID, SArray<Key>> worker_key_;
};

} // namespace GP
} // namespace PS
