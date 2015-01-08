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

  // info of the remote node
  const NodeID& id() { return node_.id(); }
  typename Node::Role role() { return node_.role(); }
  Range<Key> keyRange() { return Range<Key>(node_.key()); }
  // return number of node in this group or 1 otherwise
  int size() {
    return (role() == Node::GROUP ? exec_.group(id()).size() : 1);
  }

  // submit a message (task + data) to this remote node from the local
  // node. return the timestamp of this task. This message will be sliced into
  // this->size() messages, and each one goes to one remote node
  int submit(const MessagePtr& msg);
  int submit(const Task& task,
             const Message::Callback& recv_handle = Message::Callback());
  int submitAndWait(const Task& task,
                    const Message::Callback& recv_handle = Message::Callback());

  // submit msg[i] into node[i]. msg.size() must be equal to this->size()
  int submit(MessagePtrList& msgs);
  int submit(std::vector<Task>& tasks);
  int submitAndWait(std::vector<Task>& tasks);

  // user defined filters
  void encodeFilter(const MessagePtr& msg);
  void decodeFilter(const MessagePtr& msg);

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

 private:
  DISALLOW_COPY_AND_ASSIGN(RNode);
  FilterPtr findFilter(const FilterConfig& conf);
  Postoffice& sys_;
  Executor& exec_;
  Node node_;  // the remote node

  std::mutex mu_;
  TaskTracker incoming_task_, outgoing_task_;
  // current time
  int time_ = Message::kInvalidTime;
  // std::mutex time_mu_;

  // request messages that have been sent but not received replies yet
  std::unordered_map<int, MessagePtr> pending_msgs_;
  std::unordered_map<int, Message::Callback> msg_receive_handle_;
  std::unordered_map<int, Message::Callback> msg_finish_handle_;

  std::unordered_map<int, FilterPtr> filter_;
  std::mutex filter_mu_;
};
} // namespace PS
