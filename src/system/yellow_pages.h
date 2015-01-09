#pragma once

#include "util/common.h"
#include "proto/node.pb.h"
#include "system/van.h"

namespace PS {

class Customer;
typedef shared_ptr<Customer> CustomerPtr;
class NodeGroup;

// maintain inforamations about nodes and customers
class YellowPages {
 public:
  YellowPages() { }
  ~YellowPages();
  void init() { van_.init(); }

  // manage customers
  void addCustomer(Customer* obj);
  // ask the system to delete the customer
  void depositCustomer(Customer* obj);
  void removeCustomer(const string& name);
  Customer* customer(const string& name);

  // manage nodes
  void addNode(const Node& node);
  int num_workers() { return num_workers_; }
  int num_servers() { return num_servers_; }
  std::vector<Node> nodes();

  Van& van() { return van_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(YellowPages);
  int num_workers_ = 0;
  int num_servers_ = 0;

  std::map<NodeID, Node> nodes_;
  std::map<string, Customer*> customers_;
  std::unordered_set<string> deletable_customers_;

  Van van_;
};

} // namespace PS
