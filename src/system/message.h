#pragma once
#include "util/common.h"
#include "util/threadpool.h"
#include "util/shared_array.h"
#include "system/proto/task.pb.h"
#include "filter/proto/filter.pb.h"
namespace PS {

struct Message;
typedef std::shared_ptr<Message> MessagePtr;
typedef std::shared_ptr<const Message> MessageCPtr;
typedef std::vector<MessagePtr> MessagePtrList;
typedef std::vector<Range<Key>> KeyRangeList;
typedef std::string NodeID;

// The message communicated between nodes. It conntains all argument and data
// a request or a response needed.
struct Message {
 public:
  const static int kInvalidTime = -1;
  Message() { }
  Message(const NodeID& dest, int time = kInvalidTime, int wait_time = kInvalidTime);
  Message(const Task& tk, const NodeID& dst) : task(tk), recver(dst) { }
  explicit Message(const Task& tk) : task(tk) { }
  void MiniCopyFrom(const Message& msg);

  Task task;  // argument

  FilterConfig* AddFilter(FilterConfig::Type type);

  // keys
  bool has_key() const { return !key.empty(); }
  template <typename T>
  void set_key(const SArray<T>& key);
  void clear_key() { task.clear_has_key(); key.clear(); }
  SArray<char> key;

  // values
  template <typename T>
  void add_value(const SArray<T>& value);
  template <typename T>
  void add_value(const SArrayList<T>& value) {
    for (const auto& v : value) add_value(v);
  }
  template <typename T>
  void add_value(const std::initializer_list<SArray<T>>& value) {
    for (const auto& v : value) add_value(v);
  }
  void clear_value() { task.clear_value_type(); value.clear(); }
  std::vector<SArray<char> > value;  // values

  // clear both keys and values
  void clear_data() { clear_key(); clear_value(); }

  // memory size in bytes
  size_t mem_size();

  // -- more local control signals --
  // they will not be sent to other nodes

  NodeID sender;  // sender node id
  NodeID recver;  // receiver node id

  bool replied   = false;  // true if this message has been replied
  bool finished  = true;   // true if the request associated with this message
                           // has been finished.
  bool valid     = true;   // an invalid message will not be sent, but be marked
                           // as finished
  bool terminate = false;  // used to stop the sending thread in Postoffice.

  typedef std::function<void()> Callback;
  Callback callback;       // the callback when the associated request is finished

  // debug
  std::string ShortDebugString() const;
  std::string DebugString() const;

 private:
  // helper
  template <typename V>
  static DataType EncodeType();

};

inline MessagePtr NewMessage(const Task& task, const NodeID& recver) {
  return MessagePtr(new Message(task, recver));
}

inline MessagePtr NewMessage() { return MessagePtr(new Message()); }

inline MessagePtr NewMessage(const Message& other) {
  return MessagePtr(new Message(other));
}


template <typename T> void Message::set_key(const SArray<T>& key) {
  task.set_key_type(EncodeType<T>());
  if (has_key()) clear_key();
  task.set_has_key(true);
  this->key = SArray<char>(key);
  if (!task.has_key_range()) Range<Key>::all().to(task.mutable_key_range());
}

template <typename T> void Message::add_value(const SArray<T>& value) {
  task.add_value_type(EncodeType<T>());
  this->value.push_back(SArray<char>(value));
}

template <typename V> DataType Message::EncodeType() {
  if (std::is_same<V, uint32>::value) return DataType::UINT32;
  else if (std::is_same<V, uint64>::value) return DataType::UINT64;
  else if (std::is_same<V, int32>::value) return DataType::INT32;
  else if (std::is_same<V, int64>::value) return DataType::INT64;
  else if (std::is_same<typename std::remove_cv<V>::type, float>::value)
    return DataType::FLOAT;
  else if (std::is_same<V, double>::value) return DataType::DOUBLE;
  else if (std::is_same<V, uint8>::value) return DataType::UINT8;
  else if (std::is_same<V, int8>::value) return DataType::INT8;
  else if (std::is_same<V, char>::value) return DataType::CHAR;
  return DataType::OTHER;
}

// slice the message if msg->key is ordered, and each key corresponds to a
// fix-length value
template <typename K>
MessagePtrList SliceKeyOrderedMsg(const MessagePtr& msg, const KeyRangeList& krs) {
  // find the positions in msg.key
  size_t n = krs.size();
  std::vector<size_t> pos(n+1);
  SArray<K> key(msg->key);
  Range<Key> msg_key_range(msg->task.key_range());
  for (int i = 0; i < n; ++i) {
    if (i == 0) {
      K k = (K)msg_key_range.project(krs[0].begin());
      pos[0] = std::lower_bound(key.begin(), key.end(), k) - key.begin();
    } else {
      CHECK_EQ(krs[i-1].end(), krs[i].begin());
    }
    K k = (K)msg_key_range.project(krs[i].end());
    pos[i+1] = std::lower_bound(key.begin(), key.end(), k) - key.begin();
  }

  // split the message according to *pos*
  MessagePtrList ret(n);
  for (int i = 0; i < n; ++i) {
    ret[i] = MessagePtr(new Message());
    ret[i]->MiniCopyFrom(*msg);
    if (krs[i].setIntersection(msg_key_range).empty()) {
      // the remote node does not maintain this key range. mark this message as
      // valid, which will not be sent
      ret[i]->valid = false;
    } else {
      ret[i]->valid = true;  // must set true, otherwise this piece might not be sent
      if (key.empty()) continue;  // to void be divided by 0
      SizeR lr(pos[i], pos[i+1]);
      ret[i]->set_key(key.segment(lr));
      for (auto& v : msg->value) {
        // ret[i]->addValue(v.segment(lr * (v.size() / key.size())));
        ret[i]->value.push_back(v.segment(lr * (v.size() / key.size())));
      }
    }
  }
  return ret;
}

} // namespace PS

// inline std::ostream& operator<<(std::ostream& os, const Message& msg) {
//   return (os << msg.ShortDebugString());
// }
