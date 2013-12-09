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
  if (uid == kServers)
    return servers_;
  if (uid == kClients)
    return clients_;
  CHECK_EQ(uid, kAll);
  return all_;
}
} // namespace PS
