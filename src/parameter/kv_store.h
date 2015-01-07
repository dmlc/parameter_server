#pragma once
#include "parameter/shared_parameter.h"
namespace PS {

// key value storage. an entry *E* should has two functions get(char const* data)
// and put(char* data), where data has length *entry_sync_size*
template <typename K, typename E>
class KVStore : public SharedParameter<K> {
 public:
  // only supports fix size entry_sync_size
  // TODO if entry_sync_size == -1, then allow variable length
  KVStore(int entry_sync_size) : k_(entry_sync_size) { }
  virtual ~KVStore() {}

  // TODO multi-thread by shard
  virtual void getValue(const MessagePtr& msg) {
    SArray<K> key(msg->key);
    size_t n = key.size();
    SArray<char> val(n * k_);
    for (size_t i = 0; i < n; ++i) {
      data_[key[i]].put(val.data() + i * k_);
    }
    msg->addValue(val);
  }

  virtual void setValue(const MessagePtr& msg) {
    SArray<K> key(msg->key);
    CHECK_GT(msg->value.size(), 0);
    SArray<char> val(msg->value[0]);
    size_t n = key.size();
    CHECK_EQ(n * k_, val.size());
    for (size_t i = 0; i < n; ++i) {
      data_[key[i]].get(val.data() + i * k_);
    }
  }

  virtual MessagePtrList slice(const MessagePtr& msg, const KeyList& sep) {
    return sliceKeyOrderedMsg<K>(msg, sep);
  }

  // TODO fault tolerance
  void setReplica(const MessagePtr& msg) { }
  void getReplica(const MessagePtr& msg) { }
  void recoverFrom(const MessagePtr& msg) { }
 protected:
  int k_ = 1;
  std::unordered_map<K, E> data_;
};

} // namespace PS
