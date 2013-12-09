#include "system/dht.h"
#include "box/container.h"
namespace PS {


  const DHTInfo& DHT :: AssignNodes(Container* ctr, const KeyRange& whole, const NodeGroup& group, const map<uid_t, Node>& nodeMap) {
    HashRing hashring(num_vnode_);

    hashring.SetKeyRange(whole);
    size_t num = group.servers()->size();

    for (size_t i = 0; i < num; i++) {
      uid_t id = group.servers()->at(i);
      auto iter = nodeMap.find(id);
      std::string addr = iter->second.addr();
      std::string node_mark = strfy(id) + "#" + addr;
      if(id == 0)
        hashring.AddCoolNode(node_mark);
      else {
        CHECK_NE(i, 0);
        hashring.AddNode(node_mark, num, i);
      }
    }

    hashring.DivideKeyRange(ctr, whole, dhtinfo_);
    hashring.CollectBkpInfo(ctr, dhtinfo_);

    hashrings_[ctr->name()] = hashring;

    return dhtinfo_;
  }

}
