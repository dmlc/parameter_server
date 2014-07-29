#pragma once

#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"

namespace PS {

// An object shared by several nodes. This object is identified by an unique
// string name
class Customer {
 public:
  Customer() : sys_(Postoffice::instance()), exec_(*this) {
    exec_thread_ = unique_ptr<std::thread>(new std::thread(&Executor::run, &exec_));
  }

  // process a message received from a remote node
  virtual void process(Message* msg) = 0;

  // given the partition keys (k_1, k_2, ..., k_n), decompose the message into
  // n-1 messages such that the i-th message containing only keys and values in
  // the key range [k_i, k_{i+1})
  virtual std::vector<Message>
  decompose(const Message& msg, const Keys& partition) {
    return std::vector<Message>(partition.size()-1, msg);
  }

  void stop() {
    exec_.stop();
    exec_thread_->join();
  }

  // unique name of this customer
  const string& name() const { return name_; }
  string& name() { return name_; }

  // the uique node id running this customer
  NodeID myNodeID() { return exec_.myNode().id(); }

  Executor& exec() { return exec_; }
  RNodePtr taskpool(const NodeID& k) { return exec_.rnode(k); }

  // all child customer names
  const std::vector<string>& children() const { return child_customers_; }

 protected:
  string name_;
  std::vector<string> child_customers_;
  Postoffice& sys_;
  Executor exec_;
  unique_ptr<std::thread> exec_thread_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Customer);
};

} // namespace PS
