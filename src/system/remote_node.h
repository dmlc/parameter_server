#pragma once
#include "proto/task.pb.h"
#include "util/common.h"
#include "system/task_tracker.h"
#include "system/van.h"
#include "system/postoffice.h"
#include "filter/filter.h"
namespace PS {

class Executor;
class Postoffice;
class RNode;
typedef shared_ptr<RNode> RNodePtr;
typedef std::vector<RNodePtr> RNodePtrList;

// a remote node that the local node (the node runs executor) can submit a task to
// or receive a task from. (it is similar to remote precedure call)
class RNode {
 public:
  friend class Executor;
  RNode(const Node& node, Executor& exec)
      : sys_(Postoffice::instance()), exec_(exec), node_(node)  { }
  ~RNode() { }

  // query about this remote node info
  const NodeID& id() { return node_.id(); }
  typename Node::Role role() { return node_.role(); }
  // the key range this node maintains
  Range<Key> keyRange() { return Range<Key>(node_.key()); }


  // submit a message (task + data) to this remote node from the local
  // node. return the timestamp of this task
  int submit(const MessagePtr& msg);
  int submit(const Task& task, const Message::Callback& recv_handle = Message::Callback()) {
    MessagePtr msg(new Message(task));
    msg->recv_handle = recv_handle;
    return submit(msg);
  }

  int submitAndWait(const Task& task, const Message::Callback& recv_handle = Message::Callback()) {
    MessagePtr msg(new Message(task));
    msg->recv_handle = recv_handle;
    msg->wait = true;
    return submit(msg);
  }


  void encodeFilter(const MessagePtr& msg);
  void decodeFilter(const MessagePtr& msg);

  // cache the keys, return the message with keys removed but filled with a
  // key signature when *FLAGS_key_cache* is true
  // void cacheKeySender(const MessagePtr& msg);
  // void cacheKeyRecver(const MessagePtr& msg);
  // void clearCache() { key_cache_.clear(); }

  // wait a submitted task (send to the remote node from the local node) with
  // timestamp _time_ until it finished (received all replied message)
  void waitOutgoingTask(int time);

  // wait a received task (send from the remote node to the local node) until
  // finished (this task has been marked as finished in TaskTracker)
  void waitIncomingTask(int time);

  bool tryWaitOutgoingTask(int time);
  bool tryWaitIncomingTask(int time);

  void finishOutgoingTask(int time);
  void finishIncomingTask(int time);

  int time() { Lock l(mu_); return time_; }

  // memory usage in bytes
  // size_t memSize();
 private:
  DISALLOW_COPY_AND_ASSIGN(RNode);
  FilterPtr findFilter(const FilterConfig& conf);
  Postoffice& sys_;
  Executor& exec_;
  // the remote node
  Node node_;
  std::mutex mu_;
  TaskTracker incoming_task_, outgoing_task_;
  // current time
  int time_ = Message::kInvalidTime;
  // std::mutex time_mu_;

  // request messages that have been sent but not received replies yet
  std::unordered_map<int, MessagePtr> pending_msgs_;
  std::unordered_map<int, Message::Callback> msg_receive_handle_;
  std::unordered_map<int, Message::Callback> msg_finish_handle_;


  // // (channel, key_range) => (key signature, key list)
  // std::unordered_map<std::pair<int,Range<Key>>,
  //                    std::pair<uint32_t, SArray<char>>> key_cache_;
  // std::mutex key_cache_mu_;

  // const size_t max_sig_len_ = 2048;  // hack, because crc32 is too slow...
  std::unordered_map<int, FilterPtr> filter_;
  std::mutex filter_mu_;
};
} // namespace PS
