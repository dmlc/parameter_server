#include "system/node.h"

namespace PS {

void Node::Init(int type, int id, string net_addr, string cmd_addr) {
  id_ = id;
  net_addr_ = net_addr;
  cmd_addr_ = cmd_addr;
  type_ = type;
  uid_ = uid(type, id);
  CHECK(Valid()) << ": " << ToString();
}

uid_t Node::GetUid(const string& type, int id) {
  if (Server(type))
    return Node::uid(kTypeServer, id);
  if (Client(type))
    return Node::uid(kTypeClient, id);
  CHECK(false) << "unknow node type: " << type;
  return -101;
}

bool Node::Valid() const {
  if (!(is_client() || is_server()) || net_addr_.empty() || cmd_addr_.empty())
    return false;
  return true;
}

string Node::ShortName() const {
  string type("U");
  if (is_client()) type = "C";
  if (is_server()) type = "S";
  return StrCat(type, id_);
}

string Node::ToString() const {
  string type("Unknown_");
  if (is_client()) type = "Client_";
  if (is_server()) type = "Server_";
  string id = StrCat(type, id_, "(uid[", uid_, "]");
  return StrCat(id, "addr[", net_addr_, "], cmd_addr[", cmd_addr_, "])");
}

} // namespace PS
