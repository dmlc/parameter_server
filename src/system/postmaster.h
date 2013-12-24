#pragma once
#include "util/common.h"
#include "util/mail.h"
#include "util/blocking_queue.h"
#include "util/futurepool.h"
#include "system/workload.h"
#include "system/dht.h"
#include "system/address_book.h"
#include "proto/command.pb.h"
#include "proto/nodemgt.pb.h"

namespace PS {

class Container;
class Inference;

class NameID {
 public:
  NameID() : available_id_(0) { }
  int32 GetID(string name) {
    for (auto& i : id2name_) {
      if (i.second == name) {
        return i.first;
      }
    }
    auto id = available_id_ ++;
    id2name_[id] = name;
    return id;
  }
  string GetName(int32 id) {
    return id2name_[id];
  }
 private:
  map<int32, string> id2name_;
  int32 available_id_;
};


// a postmaster knows information about all available postoffices (nodes), it is
// also his job to manage this information, i.e.~monitor whether any node has
// die or there is new node.
class Postmaster {
 public:
  SINGLETON(Postmaster);

  // init all nodes informations, and connect them
  void Init();

  // return some commonly used node(s), make them const because only I can
  // change them
  uid_t my_uid() { return my_uid_; }
  Node& node(uid_t id) { return all_[id]; }
  Node& my_node() { return node(my_uid_); };
  Node& MyNode() { return node(my_uid_); };
  bool IamClient() { return node(my_uid_).is_client(); }
  bool IamServer() { return node(my_uid_).is_server(); }

  bool IamRoot() { return my_uid_ == 0; }
  Node& Root() { return all_[0]; }

  bool IamBackupProcess() { return IamServer() && is_backup_process_; }
  // check whether my_node is the root for container/inference name
  bool IsRoot(const string& name) { return nodegroups_[name].IsRoot(my_uid_); }

  // TODO a better way to get the whole key range of the container
  // if ifr is valid, then ctr will get all clients nodes from ifr
  // return the key range this node will maintain
  KeyRange Register(Container *ctr, KeyRange whole, Inference *ifr = NULL);

  void Register(Inference *ifr, DataRange whole);

  // get an unique id from the master
  void NameToID(const string name, CmdAck* fut);

  // get the node group associated with a container or an inference algorithm
  const NodeGroup& GetNodeGroup(const string& name) const;
  // get the workload asscoiated with a container or an inference algorithm
  Workload* GetWorkload(const string& name, uid_t id);
  Container* GetContainer(const string& name) const;
  Van* GetMailVan() { return van_; }


  void ProcessExpress(const Express& cmd);


 private:
  Postmaster() { }
  DISALLOW_COPY_AND_ASSIGN(Postmaster);

  Express Reply(const Express& req);
  Postoffce* postoffice_;

  AddressBook addr_book_;
  uid_t my_uid_;


  map<pair<string, uid_t>, Workload> workloads_;
  map<string, NodeGroup> nodegroups_;
  map<string, Container*> containers_;

  NameID name_id_;
  DHT* dht_;
  DHTInfo dhtinfo_;

};


}
