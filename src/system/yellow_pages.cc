#include "system/yellow_pages.h"
#include "system/customer.h"

namespace PS {

YellowPages::~YellowPages() {
  while (customers_.size()) {
    auto it = customers_.begin();
    if (it->second.second)  {
      delete it->second.first;  // which will call removeCustomer()
    } else {
      customers_.erase(it);
    }
  }
}

void YellowPages::addCustomer(Customer* customer) {
  CHECK_EQ(customers_.count(customer->name()), 0) << customer->name();
  customers_[customer->name()] = std::make_pair(customer, false);
}

void YellowPages::depositCustomer(const string& name) {
  CHECK_EQ(customers_.count(name), 1) << name;
  customers_[name].second = true;
}

Customer* YellowPages::customer(const string& name) {
  auto it = customers_.find(name);
  if (it == customers_.end()) return nullptr;
  return it->second.first;
}

void YellowPages::removeCustomer(const string& name) {
  auto it = customers_.find(name);
  if (it == customers_.end()) return;
  customers_.erase(it);
}

void YellowPages::addNode(const Node& node) {
  nodes_[node.id()] = node;
  // connect anyway, it's safe to connect to the same node twice
  CHECK(van_.connect(node).ok());
  if (node.role() == Node::WORKER) ++ num_workers_;
  if (node.role() == Node::SERVER) ++ num_servers_;
}

std::vector<Node> YellowPages::nodes() {
  std::vector<Node> ret;
  for (const auto& it : nodes_) {
    ret.push_back(it.second);
  }
  return ret;
}

} // namespace PS
