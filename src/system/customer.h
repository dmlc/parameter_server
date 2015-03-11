#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"
namespace PS {

// The base class of shared object in parameter server, such as an application,
// or shared parameters.
class Customer {
 public:
  // A customer must have an unique ID so that it can communicate with the
  // customer with the same ID in a remote node.
  Customer(int id);
  virtual ~Customer();

  // -- Communication APIs (thread-safe): --

  // Submits a request "task" into the customer with the same ID in a remote
  // node "recver". The "task" contains the arguments, and the "recver" is the
  // ID of the remote node, which can be a particular node "W0" or a node
  // group such as "kServerGroup".
  //
  // Returns the timestamp of this request.
  //
  // Sample usage: send a request to all worker nodes and wait until finished:
  //   Task task; task.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
  //   int ts = Submit(task, kWorkerGroup);
  //   WaitSentReq(ts, kWorkerGroup);
  int Submit(const Task& task, const NodeID& recver);


  // Submits a request message into a remote node, where "msg" contains a
  // request task and other entries such as data arrays, callbacks when replies
  // received, and other flags. Returns the timestamp of this request.
  //
  // The system may read and write the content of "msg" even after "submit"
  // returns. "msg" is a shared pointer, it will be deleted after both the caller
  // and system release the pointer.
  //
  // Sample usage: the same functionality as above:
  //   MessagePtr msg(new Message(task, kWorkerGroup));
  //   msg->wait = true;
  //   Submit(msg);
  int Submit(const MessagePtr& msg);


  // Replies the request message "msg" received from msg->sender with
  // "reply". In default, "reply" is an empty ack message.
  void Reply(const MessagePtr& msg, Task reply = Task());

  // -- Consistency APIs (thread-safe) --

  // Waits until the task with "timestamp" sent to "recver" is finished. If
  // "recver" is a single node, it blocks until a reply message with "timestamp"
  // has been received from "recver". Otherwise, it will wait a reply message
  // from each node in the node group "recver".
  void WaitSentReq(int timestamp, const NodeID& recver);

  // Wait until the request task/message with "timestamp" received from "sender" is
  // finished at this node.
  void WaitRecvReq(int timestamp, const NodeID& sender);

  // Set the request with "timestamp" sent from "sender" as been
  // finished. Typically the DAG engine in `executor_` will do it
  // automatically. But this function can mark a virtual request as finished to
  // achieve certain synchronization.
  //
  // Sample usage: data aggregation at server nodes. It has three steps: 1)
  // workers push data into servers; 2) each server aggregates the pushed data
  // from workers; 3) workers pull the aggregated data from servers.
  //
  // Implementation:
  //
  // Worker: submit a push request with timestamp t via `push_task.set_time(t)`;
  // then submit a pull request with timestamp t+2 which depends timestamp t+1,
  // namely `pull_task.set_time(t+2); pull_task.add_wait_time(t+1)`.
  //
  // Server; wait for pushed data via `WaitRecvReq(t, kWorkerGroup)`; aggregate
  // data; mark the virtual request t+1 as finished via
  // `FinishRecvReq(t+1)`. Then all blocked pull requests will be executed by
  // the DAG engine.
  void FinishRecvReq(int timestamp, const NodeID& sender);

  // -- User definable APIs --

  // Processes a message received from "msg->sender". It will be called by
  // executor_'s processing thread once the time dependencies specified in
  // "msg->task" have been satisfied.
  virtual void Process(const MessagePtr& msg) { }

  // Slices a message into multiple parts. It will be called by the system when
  // submit a task/message into a node group. Assume there are n nodes in this
  // group, which are sorted according to their key range. "krs" is the list of
  // the n key ranges.
  //
  // It should return a list of n messages such as the i-th message is sent to
  // the i-th node. In default, it copies msg n times.
  virtual MessagePtrList Slice(const MessagePtr& msg, const KeyRangeList& krs);


  // -- accessors --

  int id() const { return id_; }


  // DEPRECATED

  // process a message received from a remote node. It will be called by
  // executor's processing thread
  virtual void process(const MessagePtr& msg) { }

  // this function slices the message _msg_ into n messages such that the i-th
  // one contains only keys and values in the key range _krs[i]_. _krs_ are
  // ordered such that krs[i-1].end == krs[i].begin.
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs);

  // return the remote_note by its name
  RNode* port(const NodeID& k) { return CHECK_NOTNULL(exec_.rnode(k)); }
  Executor& exec() { return exec_; }

 protected:
  int id_;
  Postoffice& sys_;
  Executor exec_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Customer);
};

// The base class of an application.
class App : public Customer {
 public:
  App();
  virtual ~App() { }

  // -- User definable APIs --

  // The factory method. An applications must implementation this
  // function. "conf" contains the configurations in file -app_file and string
  // -app_conf.
  static App* Create(const std::string& conf);

  // `Run()` is executed by the main thread immediately after this app has ben
  // created.
  virtual void Run() { }


  // DEPRECATED
  static App* create(const std::string& conf);
  virtual void run() { }
};


}  // namespace PS
