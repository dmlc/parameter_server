#pragma once
#include "Eigen/Dense"
#include "parameter/kv_vector.h"
namespace PS {
template<typename K, typename V> class KVBufferedVector;
template<typename K, typename V>
using KVBufferedVectorPtr = std::shared_ptr<KVBufferedVector<K,V>>;

// similar to KVVector, but the received data is not write into value directly,
// instead, it is stored in a buffer, and can be read by _received(time)_
template <typename K, typename V>
class KVBufferedVector : public KVVector<K, V> {
 public:

  // return the mareged data received at time t, then *delete* it. If no
  // message, or empty messages are received at time t, then call received(t)
  // will get an error
  typedef std::pair<SizeR, std::vector<SArray<V>>> MergedData;
  MergedData received(int t);

  // override the setValue()
  void setValue(const MessagePtr& msg);
 protected:
  std::unordered_map<int, MergedData> recved_val_;
};

template <typename K, typename V>
void KVBufferedVector<K,V>::setValue(const MessagePtr& msg) {
  SArray<K> recv_key(msg->key);
  if (recv_key.empty()) return;
  int chl = msg->task.key_channel();
  auto& my_key = key(chl);
  if (msg->value.size() == 0) {
    // only keys, merge these keys, and also clear the values
    my_key = my_key.setUnion(recv_key);
    val_[chl].clear();
    return;
  }

  // merge values, and store them in recved_val
  int t = msg->task.time();
  Range<K> key_range(msg->task.key_range());
  SizeR idx_range = my_key.findRange(key_range);

  mu_.lock();
  auto& matched = recved_val_[t];
  mu_.unlock();

  for (int i = 0; i < msg->value.size(); ++i) {
    SArray<V> recv_data(msg->value[i]);
    CHECK_EQ(recv_data.size(), recv_key.size());
    bool first = matched.second.size() <= i;
    if (first) {
      // it is the first node, allocate the memory
      matched.first = idx_range;
      matched.second.push_back(SArray<V>());
      CHECK_EQ(parallelOrderedMatch(
          recv_key, recv_data, my_key.segment(idx_range),
          OpAssign<V>(), FLAGS_num_threads, &matched.second[i]), recv_key.size());
    } else {
      CHECK_EQ(matched.first, idx_range);
      CHECK_EQ(parallelOrderedMatch(
          recv_key, recv_data, my_key.segment(idx_range),
          OpPlus<V>(), FLAGS_num_threads, &matched.second[i]), recv_key.size());
    }
  }
}

} // namespace PS
