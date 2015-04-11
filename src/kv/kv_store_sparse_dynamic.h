#pragma once
#include "kv/kv_store.h"
#include "base/blob.h"
namespace ps {

// dynamic length value
template<typename K, typename V, typename Handle>
class KVStoreSparseDynamic : public KVStore {
 public:
  KVStoreSparseDynamic(int id, Handle handle)
      : KVStore(id), handle_(handle) { }
  virtual ~KVStoreSparseDynamic() {
    for (auto& v : data_) delete [] v.second.data;
  }

  void GetValue(Message* msg) {
    // parse data
    CHECK(!msg->has_key()) << "limited support yet";
    K key = msg->task.key_channel();
    CHECK_EQ(msg->task.value_len_size(), 1);
    int val_len = msg->task.value_len(0);
    // TODO zero copy
    SArray<V> val(val_len);
    Blob<V> send_val(val.data(), val_len);
    int ts = msg->task.time();

    handle_.HandlePull(
        CBlob<K>(&key, 1), CBlob<V>(FindValue(key), ts, val_len),
        &send_val);

    msg->add_value(val);
  }

  void SetValue(const Message* msg) {
    // parse data
    CHECK(!msg->has_key()) << "limited support yet";
    K key = msg->task.key_channel();
    // CHECK_EQ(msg->task.value_len_size(), 1);
    int val_len = msg->task.value_len(0);
    int ts = msg->task.time();

    CHECK_EQ(msg->value.size(), 1);
    SArray<V> val(msg->value[0]);
    CHECK_EQ(val_len val.size());

    // handle this push request
    V* val_data = val.data();
    Blob<V> my_val(FindValue(key, ts, val_len), val_len);
    handle_.HandlePush(
        CBlob<K>(&key, 1), CBlob<V>(val_data, val_len), &my_val);
  }

 private:
  V* FindValue(K key, int ts, int len) {
    auto it = data_.find(key);
    if (it == data_.end()) {
      // init if necessary
      V* data = new V[len];
      Blob<V> my_val(data, len);
      auto it2 = data_.insert(std::make_pair(key, my_val));
      CHECK(it2.second);
      it = it2.first;

      handle_.HandleInit(ts, CBlob<K>(&key, 1), &my_val);
    }
    return it->second.data();
  }

  std::unordered_map<K, Blob<V> data_;
  Handle handle_;
};

}  // namespace ps
