#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"
namespace PS{
class Postmaster;

// An object shared across multiple nodes.
class Customer {
 public:
  // A customer must have an unique name in order to communicate with the
  // customer with the same name but at a different machine.  It will inherent
  // its parent's (if not empty) node information, for example, the logisitic
  // regression application and its weight are parent and child,
  // respectively. They should have the same nodes information.
  Customer(const string& my_name, const string& parent_name = "");
  virtual ~Customer();

  // process a message received from a remote node. It will be called by
  // executor's processing thread
  virtual void process(const MessagePtr& msg) = 0;

  // this function slices the message _msg_ into n messages such that the i-th
  // one contains only keys and values in the key range _krs[i]_. _krs_ are
  // ordered such that krs[i-1].end == krs[i].begin.
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs);

  // accessor
  const string& name() const { return name_; }
  NodeID myNodeID() { return exec_.myNode().id(); }
  bool IamWorker() { return exec_.myNode().role() == Node::WORKER; }
  bool IamServer() { return exec_.myNode().role() == Node::SERVER; }
  NodeID schedulerID() { return sys_.scheduler().id(); }

  // return the executor
  Executor& exec() { return exec_; }
  // return the remote_note by its name. TODO rename it to _port_
  RNodePtr taskpool(const NodeID& k) { return exec_.rnode(k); }

 protected:
  string name_;
  Postoffice& sys_;
  Executor exec_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Customer);
};

} // namespace PS
