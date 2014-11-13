#include "parameter/shared_parameter.h"
namespace PS {

template <typename K, typename V>
class KVMap : public SharedParameter<K> {
 public:
  USING_SHARED_PARAMETER;

 public: // implement the virtual functions required
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyList& sep) {
    return sliceKeyOrderedMsg<K>(msg, sep);
  }

  // TODO multi-thread by shard
  virtual void getValue(const MessagePtr& msg) {
    SArray<K> key(msg->key);
    size_t n = key.size();
    SArray<V> val(n);
    for (size_t i = 0; i < n; ++i) {
      val[i] = data_[key[i]];
    }
    msg->addValue(val);
  }

  virtual void setValue(const MessagePtr& msg) {
    SArray<K> key(msg->key);
    CHECK_GT(msg->value.size(), 0);
    SArray<V> value(msg->value[0]);
    size_t n = key.size();
    CHECK_EQ(n, value.size());
    auto op = msg->task.shared_para().op();
    for (size_t i = 0; i < n; ++i) {
      compAssOp<V>(data_[key[i]], value[i], op);
    }
  }
  // TODO
  void setReplica(const MessagePtr& msg) { }
  void getReplica(const MessagePtr& msg) { }
  void recoverFrom(const MessagePtr& msg) { }
 protected:
  std::unordered_map<K, V> data_;
};

} // namespace PS
