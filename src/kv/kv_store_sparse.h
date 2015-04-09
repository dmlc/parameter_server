#pragma once
#include "kv/kv_store.h"
namespace ps {

template<typename K, typename V, typename Handle, int val_len>
class KVStoreSparse : public KVStore {
 public:
  KVStoreSparse(int id, Handle handle, int sync_val_len)
      : KVStore(id), handle_(handle), k_(sync_val_len) { }
  virtual ~KVStoreSparse() { }

  void GetValue(Message* msg) {
    // parse data
    SArray<K> key(msg->key);
    size_t n = key.size();
    SArray<V> val(n * k_);
    // handle this pull request
    V* val_data = val.data();
    for (size_t i = 0; i < n; ++i, val_data += k_) {
      K key_i = key[i];
      Blob<V> send_val(val_data, k_);
      handle_.HandlePush(
          CBlob<K>(&key_i, 1), CBlob<V>(data_[key_i].data(), val_len),
          &send_val);
    }
    msg->add_value(val);
  }

  void SetValue(const Message* msg) {
    // parse data
    SArray<K> key(msg->key);
    size_t n = key.size();
    CHECK_EQ(msg->value.size(), 1);
    SArray<V> val(msg->value[0]);
    CHECK_EQ(n * k_, val.size());

    // handle this push request
    V* val_data = val.data();
    for (size_t i = 0; i < n; ++i, val_data += k_) {
      K key_i = key[i];
      Blob<V> my_val(data_[key_i].data(), val_len);
      handle_.HandlePush(
          CBlob<K>(&key_i, 1), CBlob<V>(val_data, k_), &my_val);
    }
  }

 private:
  std::unordered_map<K, std::array<V, val_len>> data_;
  Handle handle_;
  int k_;
};
}  // namespace ps
