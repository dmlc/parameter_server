#pragma once

#include "util/common.h"
#include "system/node_group.h"
#include "system/van.h"

namespace PS {

DECLARE_int32(num_server);
DECLARE_int32(num_client);
DECLARE_int32(my_rank);
DECLARE_string(my_type);

class AddressBook {
 public:
  AddressBook() { }

  // init all nodes informations, and connect them
  void Init() {
    InitNodes();
    InitVans();
  }

  // nodes information
  Node& node(uid_t id) { return all_[id]; }
  Node& my_node() { return all_[my_uid_]; };
  uid_t my_uid() { return my_uid_; }
  Node& root() { return all_[0]; }

  bool IamClient() { return my_node().is_client(); }
  bool IamServer() { return my_node().is_server(); }
  bool IamRoot() { return my_uid_ == 0; }

  // vans
  Van* package_van() { return package_van_; }
  Van* express_van() { return express_van_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressBook);
  string DebugString();
  void InitNodes();
  void InitVans();
  size_t num_server_;
  size_t num_client_;
  // all available clients and servers
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
