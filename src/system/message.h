#pragma once

#include "util/common.h"
#include "base/shared_array.h"
#include "proto/task.pb.h"

namespace PS {

// typedef size_t NodeID;
typedef string NodeID;

struct Message {
  Message() { }
  explicit Message(const Task& tk) : task(tk) { }

  // content will be sent over network
  Task task;
  SArray<char> key;
  std::vector<SArray<char> > value;

  NodeID sender, recver, original_recver;

  // true if it is the first massege received in the time specified in task
  // bool first = false;

  // true if this message has been replied
  bool replied = false;
  // true if the task asscociated with this message if finished.
  bool finished = true;
  // an inivalid message will not be sent, instead, the postoffice will fake a
  // reply message. see Postoffice::queue()
  bool valid = true;

  // set it to be true to stop the sending thread of Postoffice.
  bool terminate = false;

  // clear the data, but keep all metadata
  void clearData() {
    key = SArray<char>();
    value.clear();
  }
  template <typename V>
  void addValue(const SArray<V>& val) {
    value.push_back(SArray<char>(val));
  }

  string shortDebugString() const {
    std::stringstream ss;
    ss << sender << "=>" << recver;
    if (!original_recver.empty()) ss << "(" << original_recver << ")";
    ss << ", T: " << task.time() << ", wait_T: " << task.wait_time()
       << ", " << key.size() << " keys, [" << value.size() << "] value: ";
    for (const auto& x: value)
      ss << x.size() << " ";
    ss << "[task]:" << task.ShortDebugString();
    return ss.str();
  }

  string debugString() const {
    std::stringstream ss;
    ss << "[message]: " << sender << "=>" << recver
       << "(" << original_recver << ")\n"
       << "[task]:" << task.ShortDebugString()
       << "\n[key]:" << key.size()
       << "\n[" << value.size() << " value]: ";
    for (const auto& x: value)
      ss << x.size() << " ";
    return ss.str();
  }
};

// an reply message, with empty body and task
static Message replyTemplate(const Message& msg) {
  Message reply;
  reply.sender = msg.recver;
  reply.recver = msg.sender;
  reply.task.set_customer(msg.task.customer());
  reply.task.set_request(false);
  return reply;
}

template <typename V> using AlignedArray = std::pair<SizeR, SArray<V>>;
template <typename V> using AlignedArrayList = std::vector<AlignedArray<V>>;

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



template <typename K, typename V>
Message slice(const Message& msg, const Range<K>& gr) {
  SArray<K> key(msg.key);
  SizeR lr = key.findRange(gr);
  // if (lr.empty()) {
  //   Message ret;
  //   ret.valid = false;
  //   return ret;
  // }

  Message ret = msg;
  ret.task.set_has_key(true);
  ret.key = key.segment(lr);
  ret.value.clear();
  for (auto& d : msg.value) {
    SArray<V> data(d);
    ret.value.push_back(SArray<char>(data.segment(lr)));
  }
  if (lr.empty()) ret.valid = false;
  return ret;
}

// template <typename V>
// struct AlignedSArray {
//   SizeR local;
//   std::vector<SArray<V> > data;
// };

// AlignedSArray<V> merge(const SArray<K> key, const Message& msg) {
//   AlignedSArray<V> res;

// }

// template <typename T> void add(const SArray<T> value) {
//   value_.push_back(SArray<char>(value));
// }

inline std::ostream& operator<<(std::ostream& os, const Message& msg) {
  return (os << msg.shortDebugString());
}


} // namespace PS
