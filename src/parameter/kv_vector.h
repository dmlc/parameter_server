#pragma once
#include "ps.h"
#include "parameter/parameter.h"
#include "util/parallel_ordered_match.h"
namespace PS {

// Key-value vectors. Keys are with type K, and a value is a fixed
// length array with type V, such as int[10]. Format:
//
//   key_0,   ...   key_n
//   val_00,  ...   val_n0
//   val_01,  ...   val_n1
//    ...     ...    ...
//   val_0k,  ...   val_nk
//
// Keys are ordered and unique. Both keys and values are stored in continous
// arrays to accelerate read and overwrite, where values are in a column-major
// order. However, it is efficient only when insert bulk (key,value) pairs each
// time.
//
// It supports multiple channels. Communication is isolated between
// channels. For example, we can sent a pull request on channel 1 and 2 at same
// time, then the pulled results will be store at channel 1 and 2,
// respectively.
//
//
template <typename K, typename V>
class KVVector : public Parameter {
 public:
  // k: the length of a value entry
  // buffer_value: if true then the received data in push request or a pull
  // response is merged into my value directly. otherwise, they will be aligned,
  // stored, and can be retrieved by `received'' into
  // id: customer id
  KVVector(bool buffer_value = true, int k = 1, int id = NextCustomerID()) :
      Parameter<K>(id), k_(k), buffer_value_(buffer_value) {
    CHECK_GT(k, 0);
  }
  virtual ~KVVector() { }

  // -- accessors --

  // n key-value pairs stored by arrays
  struct KVPairs {
    SArray<K> key;    // [key_0,  ..., key_n]
    SArray<V> value;  // [val_00, ..., val_0k, ..., val_n0, ..., val_nk]
  };

  // Returns the key-vale pairs in channel "chl"
  KVPairs& operator[] (int chl) { Lock l(mu_); return data_[chl]; }

  // Clears both key and value at channel "chl"
  void clear(int chl) {
    Lock l(mu_); data_[chl].key.clear(); data_[chl].value.clear();
  }

  // buffered
  struct Buffer {
    int channel;
    SizeR idx_range;
    std::vector<SArray<V>> values;
  };

  Buffer buffer(int timestamp) { Lock l(mu_); return buffer_[timestamp]; }

  void clear_buffer(int chl) {
    for (auto& v : buffer_[chl].values) v.clear();
  }

  // -- communication APIs --

  int Push(int chl,                   //
           const SArray<K>& keys,     //
           const SArray<V>& values) {
    return Push(chl, keys, {values});
  }

  int Push(int chl,
           const SArray<K>& keys,
           const std::initializer_list<SArray<V>>& values);

  // Similar to above, but assume keys in channel "chl" are already set
  int Push(int chl, const SArray<V>& values) {
    return Push(chl, SArray<K>(), values);
  }
  int Push(int chl, const std::initializer_list<SArray<V>>& values) {
    return Push(chl, SArray<K>(), values);
  }

  int Pull(int chl, const SArray<K>& keys);
  int Pull(int chl) {
    return Pull(chl, SArray<K>());
  }

  // -- implements virtual functions --
  virtual void Slice(const Message& request, const std::vector<Range<Key>>& krs,
                     std::vector<Message*>* msgs) {
    SliceKOFVMessage(request, krs, msgs);
  }
  virtual void GetValue(Message* msg) { }
  virtual void SetValue(Message* msg) { }

 protected:
  int k_;  // value entry size
  std::unordered_map<int, KVPairs> data_;  // <channel, KVPairs>

  bool buffer_value_;
  std::unordered_map<int, Buffer> buffer_;  // <channel, Buffer>

  std::mutex mu_;  // protect the structure of data_ and buffer_
};

template <typename K, typename V>
void KVVector<K,V>::SetValue(Message* msg) {
  // do check
  SArray<K> recv_key(msg->key);
  if (recv_key.empty()) return;
  int chl = msg->task.key_channel();
  mu_.lock();
  auto& kv = data_[chl];
  mu_.unlock();

  if (msg->value.size() == 0) {
    // only has keys. merge these keys
    kv.key = kv.key.setUnion(recv_key);
    // clear the values, because they are not matched any more
    kv.value.clear();
    return;
  }

  CHECK_EQ(kv.key.size() * k_, kv.value.size());
  if (kv.key.empty()) {
    LOG(ERROR) << "empty key/value at channel " << msg->task.key_channel();
    return;
  }

  for (int i = 0; i < msg->value.size(); ++i) {
    SArray<V> recv_data(msg->value[i]);
    if (buffer_value_) {
      // write the received value into kv.value directly
      CHECK_EQ(i, 0) << " can only receive one value";
      CHECK_EQ(recv_data.size(), recv_key.size() * k_);
      size_t n = ParallelOrderedMatch(recv_key, recv_val, kv.key, &kv.value);
      CHECK_EQ(n, recv_key.size());
    } else {
      // match the received value, then save it
      mu_.lock();
      auto& buf = buffer_[chl];
      mu_.unlock();

      if (i == 0) {
        SizeR idx_range = kv.key.findRange(Range<K>(msg->task.key_range()));
        if (buf.values.size() == 0) {
          // "msg" comes from the first nodes in this channel, allocate memory first
          buf.values.resize(msg->value.size());
          buf.idx_range = idx_range;
        } else {
          CHECK_EQ(buf.idx_range, idx_range);
        }
      }
      size_t k = recv_data.size() / recv_key.size();  // not necessary == k_
      size_t n = ParallelOrderedMatch(
          recv_key, recv_data, kv.key.segment(buf.idx_range), &buf.values[i], k);
      CHECK_EQ(n, recv_key.size());
    }
  }
}

template <typename K, typename V>
void KVVector<K,V>::GetValue(Message* msg) {
  // do check
  SArray<K> recv_key(msg->key);
  if (recv_key.empty()) return;
  Lock l(mu_);
  auto& kv = data_[msg->task.key_channel()];
  CHECK_EQ(kv.key.size() * k_, kv.value.size());


  // get the data
  SArray<V> val;
  size_t n = ParallelOrderedMatch(kv.key, kv.value, recv_key, &val, k_);
  msg->clear_value();
  msg->add_value(val);
}

template <typename K, typename V>
int KVVector<K,V>::Push(int chl, const SArray<K>& keys,
                        const std::initializer_list<SArray<V>>& values) {
  Lock l(mu_);
  auto& kv = data_[chl];
  if (!keys.empty()) kv.key = keys;
  Message push;
  push.set_key(kv.key);
  for (const auto& v : values) push.add_value(v);
  push.task.set_key_channel(chl);
  return Push(&push);
}

template <typename K, typename V>
int KVVector<K,V>::Pull(int chl, const SArray<K>& keys) {
  Lock l(mu_);
  if (keys.empty() ) CHECK_EQ(data_.count(chl), 1) << "empty channel " << chl;
  auto& kv = data_[chl];

  Message pull;
  pull.set_key(kv.key);
  pull.task.set_key_channel(chl);
  return Pull(pull);
}

}  // namespace PS
