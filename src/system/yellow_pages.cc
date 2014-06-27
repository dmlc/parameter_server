#include "system/yellow_pages.h"
#include "system/customer.h"
// #include "system/workload.h"

namespace PS {

void YellowPages::add(shared_ptr<Customer> customer) {
  CHECK_EQ(customers_.count(customer->name()), 0);
  customers_[customer->name()] = customer;
}

shared_ptr<Customer> YellowPages::customer(const string& name) {
  auto it = customers_.find(name);
  if (it == customers_.end())
    return shared_ptr<Customer>(nullptr);
  return it->second;
}

YellowPages::~YellowPages() {
  for (auto& it : customers_) it.second->stop();
}

} // namespace PS
