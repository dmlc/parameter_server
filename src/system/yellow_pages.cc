#include "system/yellow_pages.h"
#include "system/customer.h"
// #include "system/workload.h"

namespace PS {

void YellowPages::add(shared_ptr<Customer> customer) {
  CHECK_EQ(customers_.count(customer->name()), 0);
  customers_[customer->name()] = customer;
}

void YellowPages::add(const Node& node) {
  nodes_[node.id()] = node;
  // connect anyway, it's safe to connect to the same node twice
  CHECK(van_.connect(node).ok());
  if (node.role() == Node::WORKER) ++ num_workers_;
  if (node.role() == Node::SERVER) ++ num_servers_;
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
