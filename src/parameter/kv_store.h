#pragma once
#include "parameter/shared_parameter.h"
namespace PS {

template <typename K, typename S>
class KVState : public SharedParameter<K> {
 public:
  KVState(int id) : SharedParameter<K>(id) { }
  virtual ~KVState() {}

  void setState(const S& s) { state_ = s; }
  const S& state() { return state_; }
  virtual void writeToFile(const std::string& file) = 0;
 protected:
  S state_;
};

// key value storage.
// K: key
// V: value
// E: entry
// S: status
// an entry *E* should has two functions
// get(char const* data, S* state)
// put(char* data, S* state), where data has length *entry_sync_size*
template <typename K, typename V, typename E, typename S>
class KVStore : public KVState<K, S> {
 public:
  KVStore(int id = NextCustomerID()) : KVState<K, S>(id) { }
  virtual ~KVStore() { this->exec_.stop(); }

  using KVState<K, S>::state_;

  // TODO multi-thread by shard
  virtual void getValue(const MessagePtr& msg) {
    SArray<K> key(msg->key);
    size_t n = key.size();
    SArray<V> val(n);
    for (size_t i = 0; i < n; ++i) {
      data_[key[i]].put(val.data() + i, &state_);
    }
    msg->addValue(val);
  }

  virtual void setValue(const MessagePtr& msg) {
    SArray<K> key(msg->key);
    CHECK_GT(msg->value.size(), 0);
    SArray<V> val(msg->value[0]);
    size_t n = key.size();
    CHECK_EQ(n, val.size());
    for (size_t i = 0; i < n; ++i) {
      data_[key[i]].get(val.data() + i, &state_);
    }
    state_.update();
  }

  virtual MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs) {
    return sliceKeyOrderedMsg<K>(msg, krs);
  }

  virtual void writeToFile(const std::string& file) {
    if (!dirExists(getPath(file))) {
      createDir(getPath(file));
    }
    std::ofstream out(file); CHECK(out.good());
    V v;
    for (auto& e : data_) {
      e.second.put(&v, &state_);
      if (v != 0) out << e.first << "\t" << v << std::endl;
    }
  }
  // TODO fault tolerance
  void setReplica(const MessagePtr& msg) { }
  void getReplica(const MessagePtr& msg) { }
  void recoverFrom(const MessagePtr& msg) { }
 protected:
  std::unordered_map<K, E> data_;
};

} // namespace PS
