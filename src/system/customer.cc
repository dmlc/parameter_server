#include "system/customer.h"
namespace PS {


Customer::Customer(const string& my_name, const string& parent_name)
    : name_(my_name), sys_(Postoffice::instance()), exec_(*this) {
  CHECK(!name_.empty()) << "a customer must have a valid name";
  auto parent_ptr = sys_.yp().customer(parent_name);
  if (parent_ptr) {
    // copy parent's nodes information
    parent_ptr->addChild(my_name);
    exec_.init(parent_ptr->exec().nodes());
  }
  // register myself to system
  sys_.yp().addCustomer(this);

  exec_thread_ = unique_ptr<std::thread>(
      new std::thread(&Executor::run, &exec_));
}

Customer::~Customer() {
  sys_.yp().removeCustomer(name_);
}

MessagePtrList Customer::slice(const MessagePtr& msg, const KeyList& sep) {
  // in default, copy the message n-1 times (without the key and values)
  int m = sep.size()-1;
  MessagePtrList ret; ret.reserve(m);
  for (int i = 0; i < m; ++i) ret.emplace_back(MessagePtr(new Message(*msg)));
  return ret;
}

} // namespace PS
