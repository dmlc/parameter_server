#pragma once
#include "util/common.h"
#include "util/mail.h"
#include "util/blocking_queue.h"
#include "util/futurepool.h"
#include "system/postoffice.h"
#include "system/workload.h"
#include "system/dht.h"
#include "system/address_book.h"
#include "system/postmaster_general.h"
#include "proto/express.pb.h"
#include "proto/nodemgt.pb.h"

namespace PS {

class Container;
class Inference;

class SharedObj;

class Box;

// a postmaster knows information about all available postoffices (nodes), it is
// also his job to manage this information, i.e.~monitor whether any node has
// die or there is new node.
class Postmaster {
 public:
  SINGLETON(Postmaster);

  void Init();

  void Register(SharedObj *obj);

  // TODO a better way to get the whole key range of the container
  // if ifr is valid, then ctr will get all clients nodes from ifr
  // return the key range this node will maintain
  KeyRange Register(Container *ctr, KeyRange whole, Inference *ifr = NULL);

  //
  KeyRange Register(Box *box, KeyRange global_key_range);  // , App *app = NULL

  void Register(Inference *ifr, DataRange whole);

  // get an unique id from the master
  void NameToID(const string name, ExpressReply* fut);

  // get the node group associated with a container or an inference algorithm
  const NodeGroup& GetNodeGroup(const string& name) const;
  // get the workload asscoiated with a container or an inference algorithm
  Workload* GetWorkload(const string& name, uid_t id);
  Container* GetContainer(const string& name) const;


  void ProcessExpress(const Express& cmd);

  AddressBook* addr_book() { return &addr_book_; }
  uid_t my_uid() { return addr_book_.my_uid(); }
 private:
  Postmaster() : general_(NULL) { }
  DISALLOW_COPY_AND_ASSIGN(Postmaster);

  Express Reply(const Express& req);
  Postoffice* postoffice_;

  AddressBook addr_book_;

  // maintain the nodegroup (clients and servers) for each SharedObj
  map<int32, NodeGroup> obj_nodegroups_;
  map<int32, SharedObj*> objects_;
  map<pair<int32, uid_t>, Workload> obj_workloads_;

  map<int32, string> id2name_;
  PostmasterGeneral* general_;


  /////////////////////////
  map<pair<string, uid_t>, Workload> workloads_;
  map<string, NodeGroup> nodegroups_;
  map<string, Container*> containers_;

  DHT* dht_;
  DHTInfo dhtinfo_;

};

} // namespace PS
