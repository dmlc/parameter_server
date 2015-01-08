#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"
namespace PS {
class Postmaster;


// An object shared across multiple nodes.
class Customer {
 public:
  friend class Postmaster;
  // Customer(const string& my_name) : Customer(my_name, "") { }
  Customer(const string& my_name, const string& parent_name = "");
  virtual ~Customer();

  // process a message received from a remote node
  virtual void process(const MessagePtr& msg) = 0;

  // this function slices the message _msg_ into n messages such that the i-th
  // one contains only keys and values in the key range _krs[i]_. _krs_ are
  // ordered such that krs[i-1].end == krs[i].begin.
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs);

  // join the execution thread
  virtual void stop() { exec_.stop(); exec_thread_->join(); }

  // query my node
  const string& name() const { return name_; }
  NodeID myNodeID() { return exec_.myNode().id(); }
  NodeID schedulerID() { return sys_.scheduler().id(); }
  bool IamWorker() { return exec_.myNode().role() == Node::WORKER; }
  bool IamServer() { return exec_.myNode().role() == Node::SERVER; }

  // return the executor
  Executor& exec() { return exec_; }
  // return the remote_note by using its name
  RNodePtr taskpool(const NodeID& k) { return exec_.rnode(k); }

  // all child customer names
  const StringList& children() const { return child_customers_; }
  void addChild(const string& name) { child_customers_.push_back(name); }

 protected:
  string name_;
  StringList child_customers_;
  Postoffice& sys_;
  Executor exec_;
  unique_ptr<std::thread> exec_thread_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Customer);
};

} // namespace PS
