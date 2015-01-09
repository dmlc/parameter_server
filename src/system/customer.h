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
  // a child customer (often a shared parameter) will inherent its parent's node
  // information (oftern an app).
  Customer(const string& my_name, const string& parent_name = "");
  virtual ~Customer();

  // process a message received from a remote node
  virtual void process(const MessagePtr& msg) = 0;

  // this function slices the message _msg_ into n messages such that the i-th
  // one contains only keys and values in the key range _krs[i]_. _krs_ are
  // ordered such that krs[i-1].end == krs[i].begin.
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs);

  // query my node
  const string& name() const { return name_; }
  NodeID myNodeID() { return exec_.myNode().id(); }
  NodeID schedulerID() { return sys_.scheduler().id(); }
  bool IamWorker() { return exec_.myNode().role() == Node::WORKER; }
  bool IamServer() { return exec_.myNode().role() == Node::SERVER; }

  // return the executor
  Executor& exec() { return exec_; }
  // return the remote_note by its name
  RNodePtr taskpool(const NodeID& k) { return exec_.rnode(k); }

  // // all child customer names
  // const std::vector<string>& children() const { return child_customers_; }
  // void addChild(const string& name) { child_customers_.push_back(name); }

 protected:
  string name_;
  Postoffice& sys_;
  Executor exec_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Customer);
};

} // namespace PS
