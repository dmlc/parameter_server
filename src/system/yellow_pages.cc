#include "system/yellow_pages.h"
#include "system/customer.h"

namespace PS {

YellowPages::~YellowPages() {
  for (auto& it : customers_) {
    it.second->stop();
    if (deletable_customers_.find(it.first) != deletable_customers_.end()) {
      delete it.second;
    }
  }
}

void YellowPages::addCustomer(Customer* customer) {
  CHECK_EQ(customers_.count(customer->name()), 0) << customer->name();
  customers_[customer->name()] = customer;
}

void YellowPages::depositCustomer(Customer* obj) {
  CHECK_EQ(deletable_customers_.count(obj->name()), 0) << obj->name();
  deletable_customers_.insert(obj->name());
}

Customer* YellowPages::customer(const string& name) {
  auto it = customers_.find(name);
  if (it == customers_.end()) return nullptr;
  return it->second;
}

void YellowPages::removeCustomer(const string& name) {
  auto it = customers_.find(name);
  if (it == customers_.end()) return;
  it->second->stop();
  auto it2 = deletable_customers_.find(name);
  if (it2 != deletable_customers_.end()) {
    delete it->second;
    deletable_customers_.erase(it2);
  }
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
