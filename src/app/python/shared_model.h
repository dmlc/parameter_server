#pragma once
#include "parameter/shared_parameter.h"
#include "updater.h"

namespace PS {

DECLARE_string(app_name);

template <typename V>
class SharedModel : public SharedParameter<Key> {
  typedef Updater<V> UpdaterT;
 public:
  SharedModel(const string& my_name = FLAGS_app_name + "_model",
          const string& parent_name = FLAGS_app_name) :
      SharedParameter<Key>(my_name, parent_name) { }
  virtual ~SharedModel() { }

  void setLayer(string name, V* data, size_t size) {
    val_[name] = SArray<V>(data, size, false);
  }
  void setUpdater(UpdaterT * updater) {
    updater_ = updater;
  }

  // funcs will be called by the system
  MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs);
  void getValue(const MessagePtr& msg);
  void setValue(const MessagePtr& msg);
 protected:
  std::unordered_map<std::string, SArray<V>> val_;
  // an array is placed into multiple servers only if its length > min_slice_size
  size_t min_slice_size_ = 1000;

  UpdaterT * updater_ = nullptr;
};


template <typename V>
void SharedModel<V>::setValue(const MessagePtr& msg) {
  CHECK_EQ(msg->value.size(), 1);
  SArray<V> recv_data(msg->value[0]);
  Range<Key> kr(msg->task.key_range());
  CHECK_EQ(kr.size(), recv_data.size());
  string key = msg->task.key_channel_str();
  auto& my_val = val_[key];

  if (isWorker()) {
    if (my_val.empty()) my_val.resize(kr.size(), 0);
    CHECK_GE(my_val.size(), kr.end());
    my_val.segment(kr).copyFrom(recv_data);
  } else if (isServer()) {
    // TODO this server can do flexible consistency control here

    if (my_val.empty()) {
      // initialize weight
      my_val.resize(kr.size(), 0);
      CHECK_NOTNULL(updater_)->InitLayer(key, my_val.data(), my_val.size());
    }

    // update weight
    CHECK_GE(my_val.size(), kr.size());
    CHECK_NOTNULL(updater_)->Update(
        key, my_val.data(), recv_data.data(), recv_data.size());
  }
}

// only be called at servers, namely a worker pull data from this server
template <typename V>
void SharedModel<V>::getValue(const MessagePtr& msg) {
  auto& my_val = val_[msg->task.key_channel_str()];
  Range<Key> kr(msg->task.key_range());
  if (my_val.empty()) {
    // initialize weight
    my_val.resize(kr.size(), 0);
    CHECK_NOTNULL(updater_)->InitLayer(msg->task.key_channel_str(), my_val.data(), my_val.size());
  }

  // TODO store the kr in memory
  CHECK_EQ(my_val.size(), kr.size());
  SArray<V> send_data(kr.size());
  send_data.copyFrom(my_val);
  msg->addValue(send_data);
}

// divide a message into n part, where part i goes to server i. it's a zero-copy
// implementation
template <typename V>
MessagePtrList SharedModel<V>::slice(const MessagePtr& msg, const KeyRangeList& krs) {
  // divide the key range
  size_t n = krs.size();
  MessagePtrList ret(n);
  Range<Key> kr(msg->task.key_range());
  for (size_t i = 0; i < n; ++i) {
    ret[i] = MessagePtr(new Message());
    ret[i]->miniCopyFrom(*msg);
    ret[i]->valid = true;
    auto mut_kr = ret[i]->task.mutable_key_range();
    if (kr.size() < min_slice_size_) {
      if (i == 0) {
        // server 0 get all data
        kr.to(mut_kr);
      } else {
        Range<Key>(0,0).to(mut_kr);
        // do not sent to server 1 - n
        ret[i]->valid = false;
      }
    } else {
      kr.evenDivide(n, i).to(mut_kr);
    }
  }

  // divide the data
  for (size_t i = 0; i < msg->value.size(); ++i) {
    SArray<V> data(msg->value[i]);
    CHECK_EQ(data.size(), kr.size());
    for (size_t j = 0; j < n; ++j) {
      if (ret[j]->valid) {
        Range<Key> kr(ret[j]->task.key_range());
        ret[j]->addValue(data.segment(kr));
      }
    }
  }
  return ret;
}


} // namespace PS
