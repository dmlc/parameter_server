#pragma once

#include "util/common.h"
#include "base/shared_array.h"
#include "proto/task.pb.h"

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
  // the keys and values.
  // CAUTION: do not write data directly unless you know how it works. Use
  // key(), addKey(), value(i), addValue() instead.
  std::list<SArray<char>> data;

  bool hasKey() { return task.has_key(); }
  const SArray<char>& key() { CHECK(hasKey()); return data.front(); }
  template <typename T> void setKey(const SArray<T>& key) {
    task.set_key_type(type<T>());
    if (hasKey()) clearKey();
    task.set_has_key(true);
    data.push_front(SArray<char>(key));
    if (!task.has_key_range()) Range<Key>::all().to(task.mutable_key_range());
  }
  void clearKey() {
    if (hasKey()) { task.clear_has_key(); data.pop_front(); }
  }

  template <typename T> void addValue(const SArray<T>& value) {
    task.add_value_type(type<T>());
    data.push_back(SArray<char>(value));
  }
  int valueSize() { return data.size() - hasKey(); }
  const SArray<char>& value(int i) {
    int j = hasKey() + i; CHECK_LT(j, data.size());
    auto it = data.cbegin();
    while (j-- > 0) ++ it;
    return *it;
  }
  void clearValue() {
    task.clear_value_type();
    while (data.size() > hasKey()) data.pop_back();
  }

  // more control signals

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
  static Task::DataType type() {
    // TODc
    return Task::OTHER;
  }
 private:
};

void Message::miniCopyFrom(const Message& msg) {
  task = msg.task;
  task.clear_value_type();
  task.clear_has_key();
  terminate = msg.terminate;
  wait = msg.wait;
  recv_handle = msg.recv_handle;
  fin_handle = msg.fin_handle;
}

template <typename K>
MessagePtrList sliceKeyOrderedMsg(const MessagePtr& msg, const KeyList& sep) {
  // find the positions in msg.key
  size_t n = sep.size();
  std::vector<size_t> pos; pos.reserve(n-1);
  CHECK(msg->hasKey());
  SArray<K> key(msg->key());
  Range<K> msg_key_range(msg->task.key_range());
  for (auto p : sep) {
    K k = std::max(msg_key_range.begin(), std::min(msg_key_range.end(), (K)p));
    pos.push_back(std::lower_bound(key.begin(), key.end(), k) - key.begin());
  }

  // split the message according to *pos*
  MessagePtrList ret(n-1);
  for (int i = 0; i < n-1; ++i) {
    ret[i] = MessagePtr(new Message());
    ret[i]->miniCopyFrom(msg);
    if (Range<K>(sep[i], sep[i+1]).setIntersection(msg_key_range).empty()) {
      // the remote node does not maintain this key range. mark this message as
      // valid, which will not be sent
      ret[i]->valid = false;
    } else {
      ret[i]->valid = true;  // must set true, otherwise this piece might not be sent
      if (key.empty()) continue;  // to void be divided by 0
      SizeR lr(pos[i], pos[i+1]);
      ret[i]->addKey(key.segment(lr));
      for (auto it = msg->data.begin()+1; it != msg->data.end(); ++it) {
        ret[i]->addValue(it->segment(lr*(it->size()/key.size())));
      }
    }
  }
  return ret;
}

template <typename V> using AlignedArray = std::pair<SizeR, SArray<V>>;
template <typename V> using AlignedArrayList = std::vector<AlignedArray<V>>;

// TODO multithread version
template <typename K, typename V>
static AlignedArray<V> match(
    const SArray<K>& dst_key, const SArray<K>& src_key, V* src_val,
    Range<K> src_key_range, size_t* matched) {
  // if (src_key_range == Range<K>::all())
  //   src_key_range = src_key.range();
  *matched = 0;
  if (dst_key.empty() || src_key.empty()) {
    return std::make_pair(SizeR(), SArray<V>());
  }

  SizeR range = dst_key.findRange(src_key_range);

  SArray<V> value(range.size());
  V* dst_val = value.data();
  memset(dst_val, 0, sizeof(V)*value.size());

  // optimization, binary search the start point
  const K* dst_key_it = dst_key.begin() + range.begin();
  const K* src_key_it = std::lower_bound(src_key.begin(), src_key.end(), *dst_key_it);
  src_val += src_key_it - src_key.begin();

  while (dst_key_it != dst_key.end() && src_key_it != src_key.end()) {
    if (*src_key_it < *dst_key_it) {
      ++ src_key_it;
      ++ src_val;
    } else {
      if (!(*dst_key_it < *src_key_it)) {
        *dst_val = *src_val;
        ++ src_key_it;
        ++ src_val;
        ++ *matched;
      }
      ++ dst_key_it;
      ++ dst_val;
    }
  }
  return std::make_pair(range, value);
}

inline std::ostream& operator<<(std::ostream& os, const Message& msg) {
  return (os << msg.shortDebugString());
}

} // namespace PS
