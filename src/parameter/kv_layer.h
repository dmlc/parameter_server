#pragma once
#include "ps.h"
#include "parameter/parameter.h"
namespace PS {

/**
 * @brief the default updater for KVLayer
 */
template <typename V>
class KVLayerUpdater {
 public:
  /// @brief initialize the data
  void InitModel(int id, V* data, size_t size) {
    layer_[id] = data;
  }

  /// @brief update the model by using received data
  void Update(int id, const V* recv_data, size_t size) {
    memcpy(layer_[id], recv_data, size*sizeof(V));
  }
 private:
  std::unordered_map<int, V*> layer_;
};

/**
 * @brief multiple layers with various size
 *
 * @tparam V value type
 * @tparam Updater the updater class.
 */
template <typename V, class Updater = KVLayerUpdater<V>>
class  KVLayer : public Parameter {
 public:
  /**
   * @brief constructor
   * @param id customer id
   */
  KVLayer(size_t partition_thr = 1000, int id = NextCustomerID()) :
      Parameter(id), partition_thr_(partition_thr) { }
  virtual ~KVLayer() { }

  /// @brief set the updater,
  void set_updater(Updater* updt) { updater_ = updt; }

  /// @brief get the layer by the key
  SArray<V> layer(int key) { return layer_[key]; }

  // void set_layer(int key, V* data, size_t size) {
  //   layer_[key] = SArray<V>(data, size, false);
  // }

  /**
   * @brief Push data into servers
   *
   * @param task the request task
   * @param data layer pointer
   * @param size layer size
   * @param zero_copy if true, the system will not copy "data", which means
   * the content of "data" should not be modified or delete utill the push has
   * been successed. namely Wait() returns.
   *
   * @return the timestamp of the push request
   */
  int Push(const Task& task, V* data, size_t size, bool zero_copy = false);

  /**
   * @brief Sent a pull request to servers.
   *
   * @param task the request task
   * @param data if not NULL, then pulled back will be written into "data";
   * otherwise the received data will be saved in layer_
   * @param size layer size
   * @param callback the callback function will be called once pull is finished.
   *
   * @return the timestamp of the pull request
   */
  int Pull(const Task& task, V* data, size_t size, Message::Callback callback);

  virtual void Slice(const Message& request, const std::vector<Range<Key>>& krs,
                     std::vector<Message*>* msgs);
  virtual void GetValue(Message* msg);
  virtual void SetValue(Message* msg);
 protected:
  std::unordered_map<int, SArray<V>> layer_;
  size_t partition_thr_;
  Updater* updater_ = nullptr;
};

template <typename V, class Updater>
int KVLayer<V, Updater>::Push(const Task& task, V* data, size_t size, bool zero_copy) {
  SArray<V> val;
  if (zero_copy) {
    val = SArray<V>(data, size, false);
  } else {
    val.copyFrom(data, size);
  }
  Message push(task);
  Range<Key>(0, size).to(push.task.mutable_key_range());
  push.add_value(val);
  return Push(push);
}

template <typename V, class Updater>
int KVLayer<V, Updater>::Pull(
    const Task& task, V* data, size_t size, std::function<void()> callback) {
  int id = task.key_channel();
  if (data == NULL) {
    if (layer_[id].size() != size) layer_[id].resize(size, 0);
  } else {
    layer_[id] = SArray<V>(data, size, false);
  }
  Message pull(task);
  Range<Key>(0, size).to(pull.task.mutable_key_range());
  if (callback) pull.callback = callback;
  return Pull(pull);
}

template <typename V, class Updater>
void KVLayer<V, Updater>::Slice(
    const Message& request, const std::vector<Range<Key>>& krs,
    std::vector<Message*>* msgs) {
  // divide the key range
  size_t n = krs.size();
  int key = request.task.key_channel();
  Range<Key> kr(request.task.key_range());
  for (size_t i = 0; i < n; ++i) {
    Message* msg = (*msgs)[i];
    auto mut_kr = msg->task.mutable_key_range();
    if (kr.size() < partition_thr_) {
      // a tiny layer, sent it to server k
      int k = (key * 991) % n;
      if (i == k) {
        kr.to(mut_kr);
      } else {
        Range<Key>(0,0).to(mut_kr);
        msg->valid = false;  // invalid msg will not be sent
      }
    } else {
      // evenly parititon the data into all server nodes
      kr.evenDivide(n, i).to(mut_kr);
    }
  }

  // divide the data
  for (size_t i = 0; i < request.value.size(); ++i) {
    SArray<V> data(request.value[i]);
    CHECK_EQ(data.size(), kr.size());
    for (size_t j = 0; j < n; ++j) {
      Message* msg = (*msgs)[j];
      if (msg->valid) {
        Range<Key> kr(msg->task.key_range());
        msg->add_value(data.segment(kr));
      }
    }
  }
}

template <typename V, class Updater>
void KVLayer<V, Updater>::GetValue(Message* msg) {
  auto& my_val = layer_[msg->task.key_channel()];
  Range<Key> kr(msg->task.key_range());
  if (my_val.empty()) {
    // initialize weight
    my_val.resize(kr.size(), 0);
    CHECK_NOTNULL(updater_)->InitModel(
        msg->task.key_channel(), my_val.data(), my_val.size());
  }

  CHECK_EQ(my_val.size(), kr.size());
  SArray<V> send_data(kr.size());
  send_data.copyFrom(my_val);  // TODO, memcpy?
  msg->add_value(send_data);
}

template <typename V, class Updater>
void KVLayer<V, Updater>::SetValue(Message* msg) {
  CHECK_EQ(msg->value.size(), 1);
  SArray<V> recv_data(msg->value[0]);
  Range<Key> kr(msg->task.key_range());
  CHECK_EQ(kr.size(), recv_data.size());
  int key = msg->task.key_channel();
  auto& my_val = layer_[key];

  if (IsWorker()) {
    if (my_val.empty()) my_val.resize(kr.size(), 0);
    CHECK_GE(my_val.size(), kr.end());
    my_val.segment(kr).copyFrom(recv_data);
  } else if (IsServer()) {
    // TODO this server can do flexible consistency control here

    if (my_val.empty()) {
      // initialize weight
      my_val.resize(kr.size(), 0);
      CHECK_NOTNULL(updater_)->InitModel(key, my_val.data(), my_val.size());
    }

    // update weight
    CHECK_GE(my_val.size(), kr.size());
    CHECK_NOTNULL(updater_)->Update(key, recv_data.data(), recv_data.size());
  }
}

}  // namespace PS
