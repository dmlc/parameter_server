#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"
namespace PS {

// The base class of shared object in parameter server, such as an application,
// or shared parameters. It implements an asynchronous RPC interface. A customer
// can send a request to another customer with the same ID in a remote machine.
//
// How it works:
//
// Customer A at node NA can submit a request to customer B at node NB if both A
// and B have the same `id()'. The request message contains arguments specified
// by the protobuf class `Task' and additional data such as (key,value)
// pairs. Customer B first accepts this request via `Accept', next processes it
// by `Process', which is a user-defined function, if the dependency constraints
// are satisifed, and then sent back the responce to A by `Reply'
//
// It's an asynchronous interface. `Submit' returns immediately after the
// message is queued in the system. There is a timestamp assigned to each
// request for synchronization. For example, B can use it to check whether this
// request has been processed via `WaitAcceptedReq', while A can use
// `WaitSubmittedReq' to wait the response from B. A can also set a callback
// function when the response is arriving.
//
// Furthermore, customer A can submit a request to a node group, such as the
// server group. There is a user-defined function `Slice' which will partition
// the request message, mainly the (key,value) pairs, according to the key range
// of the remote node. Each node in this group will get a part of the original
// message.
//
// There are user-defined filters to encode and decode the messages
// communicated between nodes, mainly to reduce the network overhead.
class Customer {
 public:
  // A customer must have an unique ID so that it can communicate with the
  // customer with the same ID in a remote node.
  Customer(int id) : id_(id), sys_(Postoffice::instance()), exec_(*this) {
    sys_.manager().addCustomer(this);
  }
  virtual ~Customer() {
    sys_.manager().removeCustomer(id_);
  }

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
  //   WaitSubmittedReq(ts, kWorkerGroup);
  //   Foo();
  int Submit(const Task& task, const NodeID& recver) {
    MessagePtr ptr(new Message(task, recver));
    return Submit(ptr);
  }

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
  //   msg->callback = [this](){ Foo(); }
  //   Submit(msg);
  int Submit(const MessagePtr& msg) {
    return exec_.Submit(msg);
  }

  // Replies the request message "msg" received from msg->sender with
  // "reply". In default, "reply" is an empty ack message.
  void Reply(const MessagePtr& msg, Task reply = Task()) {
    sys_.reply(msg, reply);
  }

  // Accepts a message from a remote node
  void Accept(const MessagePtr& msg) {
    exec_.Accept(msg);
  }

  // -- Consistency APIs (thread-safe) --

  // Waits until the request with "timestamp" sent to "recver" is finished. If
  // "recver" is a single node, it blocks until a reply message with the same
  // "timestamp" has been received from "recver" or "recver" is dead. Otherwise,
  // it will wait for the responce of each alive node in the node group
  // "recver".
  void WaitSubmittedReq(int timestamp, const NodeID& recver) {
    exec_.WaitSentReq(timestamp, recver);
  }

  // Wait until the request task/message with "timestamp" received from "sender" is
  // processed at this node or "sender" is dead. If "sender" is a node group,
  // then wait for each alive node in this node group.
  void WaitAcceptedReq(int timestamp, const NodeID& sender) {
    exec_.WaitRecvReq(timestamp, sender);
  }

  // Set the request with "timestamp" sent from "sender" as been finished
  // (processed). Typically the DAG engine in `executor_` will do it
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
  void FinishAcceptedReq(int timestamp, const NodeID& sender) {
    exec_.FinishRecvReq(timestamp, sender);
  }

  // -- User definable APIs --

  // Processes a message received from "msg->sender". It will be called by
  // executor_'s processing thread once the time dependencies specified in
  // "msg->task" have been satisfied.
  virtual void Process(const MessagePtr& msg) { }

  // Slices a message into multiple parts. It will be called in `Submit` when
  // the receive node is a node group. Assume there are n nodes in this group,
  // which are sorted according to their key range. "krs" is the list of these n
  // key ranges.
  //
  // It must return a list of n messages "msgs" such as the msgs[i] is sent to
  // the i-th node. In default, it copies "msg" n times.
  virtual void Slice(
      const MessagePtr& msg, const KeyRangeList& krs, MessagePtrList* msgs) {
    for (auto& m : *msgs) *m = *msg;
  }

  // Returns the unique ID of this customer
  int id() const { return id_; }
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
  App() : Customer(Postoffice::instance().manager().nextCustomerID()) { }
  virtual ~App() { }

  // -- User definable APIs --

  // The factory method. An applications must implementation this
  // function. "conf" contains the configurations in file -app_file and string
  // -app_conf.
  static App* Create(const std::string& conf);

  // `Run()` is executed by the main thread immediately after this app has been
  // created.
  virtual void Run() { }
};


}  // namespace PS
