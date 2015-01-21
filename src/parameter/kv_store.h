#pragma once
#include "parameter/shared_parameter.h"
namespace PS {

template <typename K, typename S>
class KVState : public SharedParameter<K> {
 public:
  KVState(const string& my_name, const string& parent_name) :
      SharedParameter<K>(my_name, parent_name) { }
  virtual ~KVState() {}

  // only supports fix size entry_sync_size
  // TODO if entry_sync_size == -1, then allow variable length
  void setEntrySyncSize(int k) { k_ = k; }
  void setState(const S& s) { state_ = s; }
  const S& state() { return state_; }
  virtual void writeToFile(
      std::function<void (const K& key, char const* val)> writer) = 0;
 protected:
  int k_ = 1;
  S state_;
};

// key value storage.
// K: key
// E: entry
// S: status
// an entry *E* should has two functions
// get(char const* data, S* state)
// put(char* data, S* state), where data has length *entry_sync_size*
template <typename K, typename E, typename S>
class KVStore : public KVState<K, S> {
 public:
  KVStore(const string& my_name, const string& parent_name) :
      KVState<K, S>(my_name, parent_name) { }
  virtual ~KVStore() {}

  using KVState<K, S>::k_;
  using KVState<K, S>::state_;

  // TODO multi-thread by shard
  virtual void getValue(const MessagePtr& msg) {
    SArray<K> key(msg->key);
    size_t n = key.size();
    SArray<char> val(n * k_);
    for (size_t i = 0; i < n; ++i) {
      data_[key[i]].put(val.data() + i * k_, &state_);
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
      data_[key[i]].get(val.data() + i * k_, &state_);
    }
    state_.update();
  }

  virtual MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs) {
    return sliceKeyOrderedMsg<K>(msg, krs);
  }

  virtual void writeToFile(
      std::function<void (const K& key, char const* val)> writer) {
    char* v = new char[k_];
    for (auto& e : data_) {
      e.second.put(v, &state_);
      writer(e.first, v);
    }
    delete [] v;
  }
  // TODO fault tolerance
  void setReplica(const MessagePtr& msg) { }
  void getReplica(const MessagePtr& msg) { }
  void recoverFrom(const MessagePtr& msg) { }
 protected:
  std::unordered_map<K, E> data_;
};

} // namespace PS
