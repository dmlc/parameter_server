#include "system/node_group.h"

namespace PS {

NodeGroup::NodeGroup() :
    servers_(new std::vector<uid_t>()),
    clients_(new std::vector<uid_t>()),
    all_(new std::vector<uid_t>()) { }

bool NodeGroup::Valid(uid_t uid) {
  if (uid == kAll || uid == kClients || uid == kServers)
    return true;
  return false;
}

const NodeList& NodeGroup::Get(uid_t uid) const {
  CHECK(Valid(uid));
  // return existing group
  if (uid == kAll)
    return all_;
  else if (uid == kServers)
    return servers_;
  else if (uid == kClients)
    return clients_;
  // otherwise, insert a single node into the vector
  // NodeList node(new std::vector<uid_t>);
  // for (auto& it : *all_) {
  //   if (it == uid)
  //     node->push_back(it);
  // }
  // return node;
}
} // namespace PS
