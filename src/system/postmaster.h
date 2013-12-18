#pragma once
#include "util/common.h"
#include "util/mail.h"
#include "util/blocking_queue.h"
#include "system/node_group.h"
#include "system/workload.h"
#include "system/van.h"
#include "system/dht.h"
#include "proto/nodemgt.pb.h"

namespace PS {

DECLARE_int32(my_rank);
DECLARE_string(my_type);
DECLARE_int32(num_server);
DECLARE_int32(num_client);

// a postmaster knows information about all available postoffices (nodes), it is
// also his job to manage this information, i.e.~monitor whether any node has
// die or there is new node.  you may want postmaster to have a thread doing his
// job
class Container;
class Inference;

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
  // check whether my_node is the root for container/inference name
  bool IsRoot(const string& name) { return nodegroups_[name].IsRoot(my_uid_); }
  bool IamBackupProcess() { return IamServer() && is_backup_process_; }

  // TODO a better way to get the whole key range of the container
  // if ifr is valid, then ctr will get all clients nodes from ifr
  // return the key range this node will maintain
  KeyRange Register(Container *ctr, KeyRange whole, Inference *ifr = NULL);
  //
  void Register(Inference *ifr, DataRange whole);

  // get the node group associated with a container or an inference algorithm
  const NodeGroup& GetNodeGroup(const string& name) const;
  // get the workload asscoiated with a container or an inference algorithm
  Workload* GetWorkload(const string& name, uid_t id);
  Container* GetContainer(const string& name) const;
  Van* GetMailVan() { return van_; }


  string DebugString();
  // accept mail from postoffice
  // void Accept(const Mail& mail) {
  // mails_received_.Put(mail);
  // }
  // assign nodes to contrainer  // void AssignNodes(Container *ctr);

  // get ith_replica of replica node info, called by replica manager
  map<uid_t, KeyRange> GetReplicaTo(string name, int32 ith_replica);
  map<uid_t, KeyRange> GetReplicaTo(string name, int32 ith_replica, uid_t id);

  void send_cmd();
  void receive_cmd();

  void RescueAck(string name);


 private:
  Postmaster()  { }
  DISALLOW_COPY_AND_ASSIGN(Postmaster);

  // 15712 fault tolernce
  // non-zero master node
  std::thread* send_thread_;
  std::thread* receive_thread_;

  // called by the master node 1) notify all clients 2) load back up in servers
  // 3) send keyrange information to new generated node
  // 4) notify all client and servers of the new node added
  void DealWithFailureNode(uid_t id);

  // distuigh zero master and non zero master
  // call dht and send all these information
  void MasterAssignNodes(Container *ctr, KeyRange whole);
  void MapServerKeyRange(Container *ctr, KeyRange whole);
  void SendKeyRange(Container *ctr, uid_t id);
  void SendReplicaInfo(Container *ctr);
  // void SendKeyRangeReplica(Container *ctr);

  // receive the node list info and assign nodes
  void SlaveAssignNodes(Container *ctr, KeyRange whole);
  void ReceiveKeyRange(Container *ctr, KeyRange whole);
  void ReceiveReplicaInfo(Container *ctr);

  bool PingNode(uid_t id);
  void Ack();
  void RemoveNode(uid_t id);
  void AddNode(string name, Node node, KeyRange kr);
  void BroadcastDead(uid_t id);
  bool ActivateBackup(FailedNode *fn, string name, uid_t recver);
  void AddBackupNode(int32 id, string name, KeyRange kr);
  void BroadcastAddNode(Node nd, KeyRange kr, uid_t failed_node, string name);
  void ExecuteCmd(NodeManagementInfo mgt_info);

  // all availabe clients and servers
  NodeGroup group_;

  // this is the ground true of all nodes in this
  // system. if a node dies, or a new node comming, modify the information
  // stored here.
  map<uid_t, Node> all_;
  // unique_ptr<Node[]> all_;
  // uid_t max_uid_;

  // the uid of the node I'm in charge of
  uid_t my_uid_;

  map<pair<string, uid_t>, Workload> workloads_;
  map<string, NodeGroup> nodegroups_;
  map<string, Container*> containers_;

  // for node management info communication
  BlockingQueue<NodeManagementInfo> cmd_received_;
  BlockingQueue<NodeManagementInfo> cmd_sending_;
  BlockingQueue<NodeManagementInfo> keyrange_queue_;
  BlockingQueue<NodeManagementInfo> replicainfo_;

  // back up info
  map<tuple<string, uid_t, int32>, KeyRange> replicafrom_;
  map<tuple<string, int32, uid_t>, KeyRange> replicato_;

  // master node only
  map<tuple<string, uid_t, int32, uid_t>, KeyRange> all_replicato_;


  Van* van_;
  Van* cmd_van_;

  size_t num_server_;
  size_t num_client_;

  bool is_backup_process_;

  // For fault tolerance
  DHT* dht_;
  DHTInfo dhtinfo_;
  //map<string, DHT> dhts_;
  //size_t num_vnode;
};

}
