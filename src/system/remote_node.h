#pragma once
#include "system/proto/task.pb.h"
#include "util/common.h"
#include "system/task_tracker.h"
#include "system/van.h"
#include "system/postoffice.h"
#include "filter/filter.h"
namespace PS {

// Track a request by its timestamp. It's not thread safe, do not use it directly.
class RequestTracker {
 public:
  RequestTracker() { }
  ~RequestTracker() { }

  // Returns true if timestamp "ts" is marked as finished.
  bool IsFinished(int ts) {
    return (data_.size() > ts) && data_[ts];
  }

  // Mark time timstamp "ts" as finished.
  void Finish(int ts) {
    CHECK_LT(ts, 1000000);
    if (data_.size() <= ts) data_.resize(ts*2);
    data_[ts] = true;
  }
 private:
  std::vector<bool> data_;
};

// The presentation of a remote node which is used by Executor. It's not thread
// safe, do not use it directly.
struct RemoteNode {
 public:
  RemoteNode() { }
  ~RemoteNode();

  void EncodeFilter(const MessagePtr& msg);
  void DecodeFilter(const MessagePtr& msg);

  // -- shared info --
  Node rnode;  // the remote node
  bool alive = true;  // aliveness
  std::unordered_map<int, Filter*> filters;  // <filter_type, filter_ptr>

  // -- info of requests sent to "rnode" --
  RequestTracker sent_req_tracker;
  std::unordered_map<int, Message::Callback> sent_req_callbacks;
  std::unordered_map<int, NodeID> orig_recvers;

  // -- info of request received from "rnode" --
  RequestTracker recv_req_tracker;

 private:
  Filter* FindFilterOrCreate(const FilterConfig& conf);
};



class Executor;
class Postoffice;

// a remote node that the local node (the node runs executor) can submit a task to
// or receive a task from. (it is similar to remote precedure call)
class RNode {
 public:
  friend class Executor;
  RNode(const Node& node, Executor& exec)
      : sys_(Postoffice::instance()), exec_(exec), node_(node)  { }
  ~RNode() { }

  // id of this node
  const NodeID& id() { return node_.id(); }
  Range<Key> keyRange() { return Range<Key>(node_.key()); }

  // return number of node in this group or 1 otherwise
  int size();


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
  int submit(const std::vector<Task>& tasks,
             const Message::Callback& recv_handle = Message::Callback());
  int submitAndWait(const std::vector<Task>& tasks,
                    const Message::Callback& recv_handle = Message::Callback());

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
