#pragma once

#include "util/common.h"
#include "system/node_group.h"
#include "system/van.h"

DECLARE_int32(num_server);
DECLARE_int32(num_client);
DECLARE_int32(my_rank);
DECLARE_string(my_type);

namespace PS {

class AddressBook {
 public:
  AddressBook() { }
  void Init() {
    InitNodes();
    InitVans();
  }

  Node& my_node() { return all_[my_uid_]; };
  uid_t my_uid() { return my_uid_; }

  bool IamRoot() { return my_uid_ == 0; }
  Node& root() { return all_[0]; }

 private:
  string DebugString();
  void InitNodes();
  void InitVans();
  size_t num_server_;
  size_t num_client_;
  // all availabe clients and servers
  NodeGroup group_;
  // this is the ground true of all nodes in this
  // system. if a node dies, or a new node comming, modify the information
  // stored here.
  map<uid_t, Node> all_;

  uid_t my_uid_;

  Van* package_van_;
  Van* express_van_;
};


} // namespace PS
