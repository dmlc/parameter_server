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
  void depositCustomer(const string& name);
  void removeCustomer(const string& name);
  Customer* customer(const string& name);

  void addRelation(const string& child, const string& parent) {
    relations_[parent].push_back(child);
  }
  const std::vector<string>& childern(const string& parent) {
    return relations_[parent];
  }

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
  std::map<string, std::pair<Customer*, bool>> customers_;

  // parent vs childern
  std::unordered_map<string, std::vector<string>> relations_;
  Van van_;
};

} // namespace PS
