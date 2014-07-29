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

// a remote node that the local node (the node of executor) can submit a task to
// or receive a task from. (similar to remote precedure call)
class RNode {
 public:
  typedef std::function<void()> Callback;
  friend class Executor;

  const static int kInvalidTime = -1;

  RNode(const Node& node, Executor& exec)
      : sys_(Postoffice::instance()), exec_(exec), node_(node)  { }
  ~RNode() { }

  // query about the remote node info
  const NodeID& id() { return node_.id(); }
  typename Node::Role role() { return node_.role(); }
  // the key range this node maintains
  Range<Key> keyRange() { return Range<Key>(node_.key()); }


  // submit a task to this remote node from the local node
  //
  // msg: task info, keys, and values
  //
  // received: will be called once a reply message from this remote node has
  // been received. If this is a group node, say kServerGroup with k servers,
  // then k reply messages will be received. This callback will be calle k times
  //
  // finished: will be called once this task marked as finished. It will be
  // called only once
  //
  // no_wait: if true then return immediately else wait until this task has been
  // finished, namely received all reply messages
  //
  // return: the timestamp of this task
  int submit(Message msg, Callback received, Callback finished, bool no_wait);

  int submit(
      Message msg, Callback received = Callback(), Callback finished = Callback()) {
    return submit(msg, received, finished, true);
  }

  int submitAndWait(
      Message msg, Callback received = Callback(), Callback finished = Callback()) {
    return submit(msg, received, finished, false);
  }

  int submit(
      Task task, Callback received = Callback(), Callback finished = Callback()) {
    return submit(Message(task), received, finished, true);
  }
  int submitAndWait(
      Task task, Callback received = Callback(), Callback finished = Callback()) {
    return submit(Message(task), received, finished, false);
  }

  // cache the keys, may return the message with keys removed but filled with a
  // key signature
  Message cacheKeySender(const Message& msg);
  Message cacheKeyRecver(const Message& msg);
  void clearCache() { key_cache_.clear(); }

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

  // TODO lock guard?
  int incrClock(int delta = 1) { time_ += delta; return time_; }
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
  // std::mutex time_mu_;

  // request messages that have been sent but not received replies yet
  std::map<int, Message> pending_msgs_;

  std::map<int, Callback> msg_receive_handle_;

  std::map<int, Callback> msg_finish_handle_;

  std::unordered_map<Range<Key>, std::pair<uint32_t, SArray<char>>> key_cache_;
  std::mutex key_cache_mu_;
};

} // namespace PS
