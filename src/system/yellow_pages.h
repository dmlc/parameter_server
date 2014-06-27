#pragma once

#include "util/common.h"
#include "proto/node.pb.h"
#include "system/van.h"

namespace PS {

class Customer;
class NodeGroup;

// maintain inforamations about nodes and customers
class YellowPages {
 public:
  YellowPages() { }
  ~YellowPages();

  void init() { van_.init(); }

  void add(shared_ptr<Customer> customer);
  shared_ptr<Customer> customer(const string& name);

  void add(const Node& node) {
    nodes_[node.id()] = node;
    // connect anyway, it's safe to connect to the same node twice (i not sure)
    CHECK(van_.connect(node).ok());
  }

  Van& van() { return van_; }
  // Node& node(int uid) { return nodes_[uid]; }

 private:
  DISALLOW_COPY_AND_ASSIGN(YellowPages);

  map<NodeID, Node> nodes_;
  map<string, shared_ptr<Customer>> customers_;

  Van van_;
};

} // namespace PS
