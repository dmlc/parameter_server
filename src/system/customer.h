#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/postoffice.h"
#include "system/executor.h"
namespace PS{

// An object shared across multiple nodes.
class Customer {
 public:
  // A customer must have an unique id so that the remote node can find the
  // correct customer by using this id.
  Customer(int id);
  // System will automatically assign an id to this customer.
  Customer();
  virtual ~Customer();

  // process a message received from a remote node. It will be called by
  // executor's processing thread
  virtual void process(const MessagePtr& msg) { }

  // this function slices the message _msg_ into n messages such that the i-th
  // one contains only keys and values in the key range _krs[i]_. _krs_ are
  // ordered such that krs[i-1].end == krs[i].begin.
  virtual MessagePtrList slice(const MessagePtr& msg, const KeyRangeList& krs);

  // return the remote_note by its name
  RNode* port(const NodeID& k) { return CHECK_NOTNULL(exec_.rnode(k)); }

  // accessors
  Executor& exec() { return exec_; }
  int id() const { return id_; }

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
  App();
  virtual ~App() { }

  // the factory functionn
  static App* create(const std::string& conf);

  // run() is executed by the main thread immediately after this app has ben created.
  virtual void run() { }
};


} // namespace PS
