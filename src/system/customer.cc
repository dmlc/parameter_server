#include "system/customer.h"
#include "ps.h"
namespace PS {

Customer::Customer(int id)
    : id_(id), sys_(Postoffice::instance()), exec_(*this) {
  sys_.manager().addCustomer(this);
}

Customer::~Customer() {
  sys_.manager().removeCustomer(id_);
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

// RNode* Customer::port(const NodeID& k) {
//   if (sys_.manager().done()) exit(0);
//   return CHECK_NOTNULL(exec_.rnode(k));
// }

App::App() : Customer(NextCustomerID()) { }
} // namespace PS
