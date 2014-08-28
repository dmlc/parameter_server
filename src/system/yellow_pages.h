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

  void add(CustomerPtr customer);
  CustomerPtr customer(const string& name);

  void add(const Node& node);

  Van& van() { return van_; }

  int num_workers() { return num_workers_; }
  int num_servers() { return num_servers_; }
  const map<NodeID, Node>& nodes() { return nodes_; }
 private:
  DISALLOW_COPY_AND_ASSIGN(YellowPages);
  int num_workers_ = 0;
  int num_servers_ = 0;

  map<NodeID, Node> nodes_;
  map<string, shared_ptr<Customer>> customers_;

  Van van_;
};

} // namespace PS
