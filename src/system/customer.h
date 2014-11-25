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
  Customer() : sys_(Postoffice::instance()), exec_(*this) {
    exec_thread_ = unique_ptr<std::thread>(new std::thread(&Executor::run, &exec_));
  }
  // process a message received from a remote node
  virtual void process(const MessagePtr& msg) = 0;
  // *sep* are ordered key seperators (k_1, k_2, ..., k_n), this functin slices
  // the message *msg* into n-1 messages such that the i-th one contains
  // only keys and values in the key range [k_i, k_{i+1}).
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyList& sep) {
    // in default, copy the message n-1 times (without the key and values)
    int m = sep.size()-1;
    MessagePtrList ret; ret.reserve(m);
    for (int i = 0; i < m; ++i) ret.emplace_back(MessagePtr(new Message(*msg)));
    return ret;
  }

  // join the execution thread
  virtual void stop() { exec_.stop(); exec_thread_->join(); }

  // unique name of this customer
  void setName(const string& name) { name_ = name; }
  const string& name() const { return name_; }
  string& name() { return name_; }

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
