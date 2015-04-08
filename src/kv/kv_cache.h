#pragma once
#include "system/customer.h"
namespace ps {

template <typename K, typename V>
class KVCache : public Customer {
 public:
  KVCache(int id) : Customer(id) { }
  virtual ~KVCache() { }

  inline int Push(const SBlob<K>& keys,
                  const SBlob<V>& values,
                  const SyncOpts& opts) {
    Message msg(ParseOption(opts), kServerGroup);
    msg.set_key(SArray<K>(keys));
    msg.add_value(SArray<V>(values));
    if (opts.callback) msg.callback = opts.callback;
    msg.task.mutable_param()->set_push(true);
    return Submit(&msg);
  }

  inline int Pull(const SBlob<Key>& keys,
                  SBlob<V>* values,
                  const SyncOpts& opts) {
    Message msg(ParseOption(opts), kServerGroup);
    int ts = msg.task.time();
    auto& kv = pull_data_[ts];
    kv.key = SArray<K>(keys);
    kv.value = SArray<V>(*values);
    msg.set_key(kv.key);
    msg.callback = [ts, opts, this] {
      opts.callback();
      pull_data_.erase(ts);
    };
    msg.task.mutable_param()->set_push(false);
    return Submit(&msg);
  }

  void Slice(const Message& request, const std::vector<Range<Key>>& krs,
             std::vector<Message*>* msgs) {
    SliceKOFVMessage<K>(request, krs, msgs);
  }

  void SetValue(const Message* msg) {
    // received kv
    CHECK_EQ(msg->value.size(), 1);
    SArray<K> recv_key(msg->key);
    SArray<V> recv_data(msg->value[0]);
    int k = recv_data.size() / recv_key.size();

    // local kv
    auto& kv = pull_data_[msg->task.time()];
    CHECK_EQ(kv.value.size(), kv.key.size() * k);

    // match
    size_t n = ParallelOrderedMatch(
        recv_key, recv_data, kv.key, &kv.value, k, AssignOpType::ASSIGN);
    CHECK_EQ(n, recv_data.size());
  }

 private:
  Task ParseOption(const SyncOpts opts) {
    return Task();
  }

  struct KVPair {
    SArray<K> key;    // [key_0,  ..., key_n]
    SArray<V> value;  // [val_00, ..., val_0k, ..., val_n0, ..., val_nk]
  };
  std::unordered_map<int, KVPair> pull_data_;
};
}  // namespace ps
