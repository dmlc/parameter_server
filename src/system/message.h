#pragma once

#include "util/common.h"
#include "base/shared_array.h"
#include "proto/task.pb.h"

namespace PS {

typedef std::string NodeID;

struct Message;
typedef std::shared_ptr<Message> MessagePtr;
typedef std::vector<MessagePtr> MessagePtrList;

struct Message {
  const static int kInvalidTime = -1;
  // If *time* != -1, then will set the timestamp of this task into *time*
  // rather than an auto-generated time. *wait_time* is the timestamp of the
  // task this submitted task must wait. No wait if it is -1.
  Message() { }
  Message(const NodeID& dest, int time, int wait_time = kInvalidTime);
  explicit Message(const Task& tk) : task(tk) { }
  // copy all entries excepts for keys and values
  // explicit Message(const Message& msg);

  // task, key, and value will be sent over network. while the rest are only
  // used by local process/node.
  // a protobuf header, see proto/task.proto
  Task task;
  // a list of keys
  SArray<char> key;
  // the according lists of values
  std::vector<SArray<char>> value;

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

  // add the key list and the lists of values
  template <typename K, typename V>
  void addKV(const SArray<K>& k, const std::initializer_list<SArray<V>>& v) {
    key = SArray<char>(k);
    for (const auto& w : v) addValue(w);
  }
  template <typename V> void addValue(const SArray<V>& val) {
    value.push_back(SArray<char>(val));
  }
  // clear the keys and values
  void clearKV() {
    key = SArray<char>();
    value.clear();
  }

  // debug
  std::string shortDebugString() const;
  std::string debugString() const;
};


// // an reply message, with empty body and task
// static Message replyTemplate(const Message& msg) {
//   Message reply;
//   reply.sender = msg.recver;
//   reply.recver = msg.sender;
//   reply.task.set_customer(msg.task.customer());
//   reply.task.set_request(false);
//   return reply;
// }

template <typename V> using AlignedArray = std::pair<SizeR, SArray<V>>;
template <typename V> using AlignedArrayList = std::vector<AlignedArray<V>>;

// TODO multithread version
template <typename K, typename V>
static AlignedArray<V> match(const SArray<K>& dst_key,
                             const SArray<K>& src_key,
                             V* src_val,
                             Range<K> src_key_range,
                             size_t* matched) {
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



// template <typename K, typename V>
// Message slice(const Message& msg, const Range<K>& gr) {
//   SArray<K> key(msg.key);
//   SizeR lr = key.findRange(gr);
//   // if (lr.empty()) {
//   //   Message ret;
//   //   ret.valid = false;
//   //   return ret;
//   // }

//   Message ret = msg;
//   ret.task.set_has_key(true);
//   ret.key = key.segment(lr);
//   ret.value.clear();
//   for (auto& d : msg.value) {
//     SArray<V> data(d);
//     ret.value.push_back(SArray<char>(data.segment(lr)));
//   }
//   if (lr.empty()) ret.valid = false;
//   return ret;
// }


inline std::ostream& operator<<(std::ostream& os, const Message& msg) {
  return (os << msg.shortDebugString());
}

} // namespace PS
