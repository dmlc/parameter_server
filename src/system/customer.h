#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"
namespace PS {

// The base class of shared object in parameter server, such as an application,
// or shared parameters. It implements an asynchronous RPC-like interface. A
// customer can send a request to another customer with the same ID in a remote
// machine.
//
// How it works:
//
// Customer A at node NA can submit a request to customer B at node NB if both A
// and B have the same `id()'. The request message, class `Message', contains arguments specified
// by the protobuf class `Task' and additional data such as (key,value)
// pairs. Customer B first accepts this request via `Accept'; next processes it
// by `ProcessRequest', which is a user-defined function, if the dependency constraints
// are satisifed; and then sent back the responce to A by `Reply'. Once A
// received the process, `ProcessResponse' will be called.
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

  // -- APIs for the caller (thread-safe) --

  // Submits a request "task" into the customer with the same `id()' in a remote
  // node "recver". The "task" contains the arguments, and the "recver" is the
  // ID of the remote node, which can be a particular node "W0" or a node
  // group such as "kServerGroup".
  //
  // Returns the timestamp of this request.
  //
  // Sample usage: send a request to all worker nodes and wait until finished:
  //   Task task; task.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
  //   int ts = Submit(task, kWorkerGroup);
  //   Wait(ts, kWorkerGroup);
  //   Foo();
  inline int Submit(const Task& request, const NodeID& recver) {
    return Submit(NewMessage(request, recver));
  }

  // Submits a request message into a remote node, where "request" contains a
  // request task and other entries such as data arrays, callbacks when replies
  // received, and other flags. Returns the timestamp of this request.
  //
  // The system may read and write the content of "request" even after "submit"
  // returns. "request" is a shared pointer, it will be deleted after both the caller
  // and system release the pointer.
  //
  // Sample usage: the same functionality as above:
  //   auto req = NewMessage(task, kWorkerGroup);
  //   reg->callback = [this](){ Foo(); }
  //   Submit(msg);
  inline int Submit(const MessagePtr& request) {
    return exec_.Submit(request);
  }

  // Waits until the request with "timestamp" sent to "recver" is finished. If
  // "recver" is a single node, it blocks until a reply message with the same
  // "timestamp" has been received from "recver" or "recver" is dead. Otherwise,
  // it will wait for the responce of each alive node in the node group
  // "recver".
  inline void Wait(int timestamp, const NodeID& recver) {
    exec_.WaitSentReq(timestamp, recver);
  }

  // Slices a message into multiple parts. It will be called in `Submit` when
  // the receive node is a node group. Assume there are n nodes in this group,
  // which are sorted according to their key range. "krs" is the list of these n
  // key ranges.
  //
  // It must return a list of n messages "msgs" such as the msgs[i] is sent to
  // the i-th node. In default, it copies "msg" n times.
  virtual void Slice(
      const MessagePtr& request, const KeyRangeList& krs, MessagePtrList* msgs) {
    for (auto& m : *msgs) m = NewMessage(*request);
  }

  // Processes a response message received form "response->sender". It is a
  // user-defined function, which will be called by the executor's processing
  // thread.
  virtual void ProcessResponse(const MessagePtr& response) { }

  MessagePtr LastResponse() {
    return exec_.activeMessage();
  }

  // -- APIs for callee (thread-safe) --

  // Processes a request message received from "request->sender". It is a
  // user-defined function, which will be called by the executor's processing
  // thread once the time dependencies specified in "request->task" have been
  // satisfied.
  virtual void ProcessRequest(const MessagePtr& request) { }

  MessagePtr LastRequest() {
    return exec_.activeMessage();
  }

  // Wait until the request task/message with "timestamp" received from "sender" is
  // processed at this node or "sender" is dead. If "sender" is a node group,
  // then wait for each alive node in this node group.
  inline void WaitReceivedRequest(int timestamp, const NodeID& sender) {
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
  inline void FinishReceivedRequest(int timestamp, const NodeID& sender) {
    exec_.FinishRecvReq(timestamp, sender);
  }


  // Replies the request message "msg" received from msg->sender with
  // "reply". In default, "reply" is an empty ack message.
  void Reply(const MessagePtr& request, Task response = Task()) {
    sys_.reply(request, response);
  }

  void Reply(const MessagePtr& request, MessagePtr response) {
    // TODO
  }


  // Returns the unique ID of this customer
  int id() const { return id_; }

  // Returns the executor of this customer.
  Executor* executor() { return &exec_; }

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
  virtual void Run() {
    // in default, just wait all nodes are ready.
    sys_.manager().waitServersReady();
    sys_.manager().waitWorkersReady();
  }
};


}  // namespace PS
