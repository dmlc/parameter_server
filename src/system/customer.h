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

  void stop() {
    exec_.stop();
    exec_thread_->join();
  }

  // process a message received from a remote node
  virtual void process(Message* msg) = 0;

  // decompose the message give the partition keys
  virtual std::vector<Message>
  decompose(const Message& msg, const Keys& partition) {
    return std::vector<Message>(partition.size()-1, msg);
  }

  // accessors and mutators
  const string& name() const { return name_; }
  string& name() { return name_; }

  NodeID myNodeID() { return exec_.myNode().id(); }

  const std::vector<string> child_customers() const { return child_customers_; }

  Executor& exec() { return exec_; }

  RNodePtr taskpool(const NodeID& k) { return exec_.rnode(k); }


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
