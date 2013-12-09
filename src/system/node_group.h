#pragma once

#include "system/node.h"

namespace PS {

// a list of nodes, a light data struture used to passing between classes
typedef shared_ptr<std::vector<uid_t> > NodeList;

// store some often used groups
class NodeGroup {
 public:
  NodeGroup();
  // notation of nodes group, always < 0 to avoid confliciting with uid
  static const uid_t kInvalid = -1;
  static const uid_t kAll= -2;
  static const uid_t kClients = -3;
  static const uid_t kServers = -4;
  // whether or not uid is a valid group uid
  static bool Valid(uid_t uid);
  // whether is the root node of this group
  bool IsRoot(uid_t id) { return id == root_; }

  const NodeList& Get(uid_t uid) const;

  // accessor and mutators
  NodeList& all() { return all_; }
  const NodeList& all() const { return all_; }
  NodeList& servers() { return servers_; }
  const NodeList& servers() const { return servers_; }
  NodeList& clients() { return clients_; }
  const NodeList& clients() const { return clients_; }
  void set_root(uid_t id) { root_ = id; }
  uid_t root() const { return root_; }
 private:
  // some predefined node groups, all = servers + clients.
  NodeList servers_;
  NodeList clients_;
  NodeList all_;
  uid_t root_;
};

} // namespace PS
