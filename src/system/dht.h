#pragma once

#include "system/hashring.h"
#include "system/node_group.h"
#include "system/node.h"
#include "util/key.h"

namespace PS {

class Container;

class DHT {
  public:
    DHT(size_t num_vnode) : num_vnode_(num_vnode) {
    }
    ~DHT() {
    }

    // assign virtual nodes to ring
    // DHTInfo AssignNodes(Container *ctr);
    const DHTInfo& AssignNodes(Container* ctr, const KeyRange& whole, const NodeGroup& group, const map<uid_t, Node>& nodeMap);

  private:
    size_t num_vnode_;
    map<string, HashRing> hashrings_;
    DHTInfo dhtinfo_;
};

}
