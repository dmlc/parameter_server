#pragma once

#include "util/common.h"
#include "util/threadpool.h"
#include "base/shared_array.h"
#include "proto/task.pb.h"
#include "proto/filter.pb.h"
#include "proto/common.pb.h"

namespace PS {

typedef std::string NodeID;

struct Message;
typedef std::shared_ptr<Message> MessagePtr;
typedef std::shared_ptr<const Message> MessageCPtr;

typedef std::vector<MessagePtr> MessagePtrList;

struct Message {
 public:
  const static int kInvalidTime = -1;
  Message() { }
  Message(const NodeID& dest, int time = kInvalidTime, int wait_time = kInvalidTime);
  explicit Message(const Task& tk) : task(tk) { }
  void miniCopyFrom(const Message& msg);

  // the header of the message, containing all metadata
  Task task;
  // key and value arrays. You'd better use setKey() and addValue() when add
  // key/value arrays
  SArray<char> key;
  std::vector<SArray<char> > value;

  // functions manipulate task, key and values.
  FilterConfig* addFilter(FilterConfig::Type type);
  // return task.has_key();
  bool hasKey() const { return !key.empty(); }
  template <typename T> void setKey(const SArray<T>& key);
  void clearKey() { task.clear_has_key(); key.clear(); }

  template <typename T> void addValue(const SArray<T>& value);

  template <typename T> void addValue(const SArrayList<T>& value) {
    for (const auto& v : value) addValue(v);
  }
  template <typename T> void addValue(const std::initializer_list<SArray<T>>& value) {
    for (const auto& v : value) addValue(v);
  }
  void clearValue() { task.clear_value_type(); value.clear(); }
  void clearData() { clearKey(); clearValue(); }

  // more control signals, which are only used by local process

  // sender node id
  NodeID sender;
  // receiver node id
  NodeID recver;
  // the original receiver node id. for example, when a worker send a message to
  // the server group (kServerGroup), then the message going to a particular
  // server will have kServerGroup as its *original_recver*
  NodeID original_recver;

  // true if this message has been replied, to avoid double reply
  bool replied = false;
  // true if the task asscociated with this message has been finished.
  bool finished = true;
  // an inivalid message will not be sent, instead, the postoffice will fake a
  // reply message. see Postoffice::queue()
  bool valid = true;
  // set it to be true to stop the sending thread of Postoffice.
  bool terminate = false;
  // wait or not when submit this message
  bool wait = false;

  typedef std::function<void()> Callback;
  // *recv_handle* will be called if anythings goes back from the destination
  // node. When called, this task has not been marked as finished. If could be
  // called multiple time when the destination node is a node group.
  Callback recv_handle;
  // *fin_handle* will be called when this task has been finished. If the dest
  // node is a node group, then it means replies from all nodes in this group
  // have been received.
  Callback fin_handle;


  // debug
  std::string shortDebugString() const;
  std::string debugString() const;

  // helper
  template <typename V>
  static DataType type() {
    // TODO
    return DataType::OTHER;
  }
 private:
};


template <typename T> void Message::setKey(const SArray<T>& key) {
  task.set_key_type(type<T>());
  if (hasKey()) clearKey();
  task.set_has_key(true);
  this->key = SArray<char>(key);
  if (!task.has_key_range()) Range<Key>::all().to(task.mutable_key_range());
}

template <typename T> void Message::addValue(const SArray<T>& value) {
  task.add_value_type(type<T>());
  this->value.push_back(SArray<char>(value));
}

template <typename K>
MessagePtrList sliceKeyOrderedMsg(const MessagePtr& msg, const KeyList& sep) {
  // find the positions in msg.key
  size_t n = sep.size();
  std::vector<size_t> pos; pos.reserve(n-1);
  SArray<K> key(msg->key);
  Range<K> msg_key_range(msg->task.key_range());
  for (auto p : sep) {
    K k = std::max(msg_key_range.begin(), std::min(msg_key_range.end(), (K)p));
    pos.push_back(std::lower_bound(key.begin(), key.end(), k) - key.begin());
  }

  // split the message according to *pos*
  MessagePtrList ret(n-1);
  for (int i = 0; i < n-1; ++i) {
    ret[i] = MessagePtr(new Message());
    ret[i]->miniCopyFrom(*msg);
    if (Range<K>(sep[i], sep[i+1]).setIntersection(msg_key_range).empty()) {
      // the remote node does not maintain this key range. mark this message as
      // valid, which will not be sent
      ret[i]->valid = false;
    } else {
      ret[i]->valid = true;  // must set true, otherwise this piece might not be sent
      if (key.empty()) continue;  // to void be divided by 0
      SizeR lr(pos[i], pos[i+1]);
      ret[i]->setKey(key.segment(lr));
      for (auto& v : msg->value) {
        ret[i]->addValue(v.segment(lr * (v.size() / key.size())));
      }
    }
  }
  return ret;
}

inline std::ostream& operator<<(std::ostream& os, const Message& msg) {
  return (os << msg.shortDebugString());
}

} // namespace PS
