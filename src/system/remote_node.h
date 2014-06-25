#pragma once

#include "proto/task.pb.h"
#include "util/common.h"
#include "system/task_tracker.h"
#include "system/van.h"
#include "system/postoffice.h"

namespace PS {


class Executor;
class Postoffice;
class RNode;

typedef shared_ptr<RNode> RNodePtr;
typedef std::vector<RNodePtr> RNodePtrList;

// a remote node associated with a customer. one can send task to this remote
// worker, and receive task from it
class RNode {
 public:
  typedef std::function<void()> Callback;
  friend class Executor;

  const static int kInvalidTime = -1;

  RNode(const Node& node, Executor& exec)
      : sys_(Postoffice::instance()), exec_(exec), node_(node)  { }
  ~RNode() { }

  // Node& node() { return node_; }
  // query about the remote node info
  NodeID id() { return node_.id(); }
  typename Node::Role role() { return node_.role(); }
  Range<Key> keyRange() { return Range<Key>(node_.key()); }

  int submit(
      Message msg, Callback before = Callback(), Callback after = Callback()) {
    return submit(msg, before, after, true);
  }

  int submitAndWait(
      Message msg, Callback before = Callback(), Callback after = Callback()) {
    return submit(msg, before, after, false);
  }

  int submit(
      Task task, Callback before = Callback(), Callback after = Callback()) {
    return submit(Message(task), before, after, true);
  }
  int submitAndWait(
      Task task, Callback before = Callback(), Callback after = Callback()) {
    return submit(Message(task), before, after, false);
  }

  int submit(Message msg, Callback before, Callback after, bool no_wait);

  // cache the keys
  // Message cache(const Message& msg);

  Message cacheKeySender(const Message& msg);
  Message cacheKeyRecver(const Message& msg);

  void clearCache() { key_cache_.clear(); }

  void waitOutgoingTask(int time);
  void waitIncomingTask(int time);

  bool tryWaitOutgoingTask(int time);
  bool tryWaitIncomingTask(int time);

  void finishOutgoingTask(int time);
  void finishIncomingTask(int time);

  // // the received request tasks, need to execute them, and send back results (or ack)
  // TaskTracker& inTask() { return in_task_; }

  // // request tasks that sent out, it will be finished if received the results
  // TaskTracker& outTask() { return out_task_; }

  int incrClock(int delta = 1) { Lock l(mu_); time_ += delta; return time_; }
  int time() { return incrClock(0); }
 private:
  DISALLOW_COPY_AND_ASSIGN(RNode);

  Postoffice& sys_;
  Executor& exec_;

  // the remote node
  Node node_;

  std::mutex mu_;

  TaskTracker incoming_task_, outgoing_task_;

  // current time
  int time_ = kInvalidTime;
  std::mutex time_mu_;

  // request messages that have been sent but not received replies yet
  std::map<int, Message> pending_msgs_;

  // will be called once a reply message have been received. If a worker send a
  // message to k servers, then it will receive k reply messages. This callback
  // will be call k times
  std::map<int, Callback> msg_receive_handle_;

  // will be called once a reply message have been finished. If a worker send a
  // message to k servers. This callback will be called only once, after all k
  // reply messages have been received.
  std::map<int, Callback> msg_finish_handle_;

  std::unordered_map<Range<Key>, std::pair<uint32_t, SArray<char>>> key_cache_;
  std::mutex key_cache_mu_;
};

} // namespace PS

  // Postoffice& sys() { return sys_; }
