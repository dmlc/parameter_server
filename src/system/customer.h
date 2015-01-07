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

  // *sep* are ordered key seperators (k_1, k_2, ..., k_n), this functin slices
  // the message *msg* into n-1 messages such that the i-th one contains
  // only keys and values in the key range [k_i, k_{i+1}).
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyList& sep);

  // join the execution thread
  virtual void stop() { exec_.stop(); exec_thread_->join(); }

  // unique name of this customer
  // void setName(const string& name) { name_ = name; }
  const string& name() const { return name_; }
  // string& name() { return name_; }

  // the uique node id running this customer
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

  // void showMem() { LL << myNodeID() << " is using " << ResUsage::myPhyMem() << " Mbytes memory"; }
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
