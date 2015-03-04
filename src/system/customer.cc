#include "system/customer.h"
namespace PS {

Customer::Customer(const string& my_name)
    : name_(my_name), sys_(Postoffice::instance()), exec_(*this) {
  CHECK(!name_.empty()) << "a customer must have a valid name";
  // register myself to system
  sys_.manager().addCustomer(this);
}

Customer::~Customer() {
  sys_.manager().removeCustomer(name_);
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
