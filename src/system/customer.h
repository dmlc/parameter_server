#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"
namespace PS {

// An object shared across multiple nodes.
class Customer {
 public:
  Customer() : sys_(Postoffice::instance()), exec_(*this) {
    exec_thread_ = unique_ptr<std::thread>(new std::thread(&Executor::run, &exec_));
  }
  // process a message received from a remote node
  virtual void process(const MessagePtr& msg) = 0;
  // *seg* are ordered keys (k_1, k_2, ..., k_n), slice message *msg* into n-1
  // messages such that the i-th message containing only keys and values in the
  // key range [k_i, k_{i+1}).
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyList& seg) {
    // in default, copy the message n-1 times
    return MessagePtrList(partition.size()-1, msg);
  }

  // join the execution thread
  void stop() { exec_.stop(); exec_thread_->join(); }

  // unique name of this customer
  const string& name() const { return name_; }
  string& name() { return name_; }
  // the uique node id running this customer
  NodeID myNodeID() { return exec_.myNode().id(); }
  // return the executor
  Executor& exec() { return exec_; }
  // return the remote_note by using its name
  RNodePtr taskpool(const NodeID& k) { return exec_.rnode(k); }
  // all child customer names
  const StringList& children() const { return child_customers_; }

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
