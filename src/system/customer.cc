#include "system/customer.h"
namespace PS {

Customer::Customer(const string& my_name, const string& parent_name)
    : name_(my_name), sys_(Postoffice::instance()), exec_(*this) {
  CHECK(!name_.empty()) << "a customer must have a valid name";
  auto parent_ptr = sys_.yp().customer(parent_name);
  if (parent_ptr) {
    // copy parent's nodes information
    parent_ptr->addChild(my_name);
    exec_.copyNodesFrom(parent_ptr->exec());
  }
  // register myself to system
  sys_.yp().addCustomer(this);

  exec_thread_ = unique_ptr<std::thread>(
      new std::thread(&Executor::run, &exec_));
}

Customer::~Customer() {
  sys_.yp().removeCustomer(name_);
}

MessagePtrList Customer::slice(const MessagePtr& msg, const KeyRangeList& krs) {
  // in default, copy the message n times
  int n = krs.size();
  MessagePtrList ret; ret.reserve(n);
  for (int i = 0; i < n; ++i) {
    ret.emplace_back(MessagePtr(new Message(*msg)));
  }
  return ret;
}

} // namespace PS
