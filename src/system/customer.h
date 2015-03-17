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
// and B have the same `id()'. The request message, class `Message', contains
// arguments specified by the protobuf class `Task' and additional data such as
// (key,value) pairs. Customer B first processes this request by the
// user-defined function `ProcessRequest' if the dependency constraints in this
// request are satisfied; and then sent back the response to A by `Reply'. Once
// A received the process, the user-defined function `ProcessResponse' will be
// called.
//
// It's an asynchronous interface. `Submit' returns immediately after the
// message is queued in the system. There is a timestamp assigned to each
// request for synchronization. For example, B can use it to check whether this
// request has been processed via `WaitReceivedRequest', while A can use `Wait'
// to wait the response from B. A can also set a callback function when the
// response is arriving.
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
    sys_.manager().AddCustomer(this);
  }
  virtual ~Customer() {
    sys_.manager().RemoveCustomer(id_);
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
  //   Wait(ts);
  //   Foo();
  inline int Submit(const Task& request, const NodeID& recver) {
    Message msg(request, recver);
    return Submit(&msg);
  }

  // Submits a request message into a remote node, where "request" contains a
  // request task and other entries such as data arrays, callbacks when replies
  // received, and other flags. Returns the timestamp of this request.
  //
  // The system may write the content of "request".
  //
  // Sample usage: the same functionality as above:
  //   Message req(task, kWorkerGroup);
  //   reg.callback = [this](){ Foo(); }
  //   Wait(Submit(&req));
  inline int Submit(Message* request) {
    return exec_.Submit(request);
  }

  // Waits until the submitted request with "timestamp" is finished. If the
  // receiver is this request is a single node, this function is blocked until a
  // reply message with the same "timestamp" has been received from this
  // receiver, or it is considered as deed.  Otherwise, this function will wait
  // for the response from each alive node in the receiver node group.
  inline void Wait(int timestamp) {
    exec_.WaitSentReq(timestamp);
  }

  // Slices a message into multiple parts. It will be called in `Submit`. Assume
  // there are n nodes in this group, which are sorted according to their key
  // range. "krs" is the list of these n key ranges.
  //
  // It must return a list of n messages "msgs" such as the msgs[i] is sent to
  // the i-th node. Each msgs[i] is already initilized by msgs[i]->task = msg.task
  virtual void Slice(const Message& request, const std::vector<Range<Key>>& krs,
                     std::vector<Message*>* msgs) { }

  // Processes a response message received form "response->sender". It is a
  // user-defined function, which will be called by the executor's processing
  // thread.
  virtual void ProcessResponse(Message* response) { }

  // The last received response.
  inline std::shared_ptr<Message> LastResponse() {
    return exec_.last_response();
  }

  // -- APIs for the callee (thread-safe) --

  // Processes a request message received from "request->sender". It is a
  // user-defined function, which will be called by the executor's processing
  // thread once the time dependencies specified in "request->task" have been
  // satisfied.
  virtual void ProcessRequest(Message* request) { }

  // The last received request.
  inline std::shared_ptr<Message> LastRequest() {
    return exec_.last_request();
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
  void Reply(Message* request, Task response = Task()) {
    Message* msg = new Message(response);
    Reply(request, msg);
  }

  // the system will delete response, so do not delete it.
  void Reply(Message* request, Message* response) {
    exec_.Reply(request, response);
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
  App() : Customer(Postoffice::instance().manager().NextCustomerID()) { }
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
    sys_.manager().WaitServersReady();
    sys_.manager().WaitWorkersReady();
  }
};


}  // namespace PS
