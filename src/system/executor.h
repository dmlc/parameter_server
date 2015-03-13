#pragma once
#include "system/remote_node.h"
#include "system/message.h"
namespace PS {

const static NodeID kGroupPrefix  = "all_";
// all server nodes
const static NodeID kServerGroup  = kGroupPrefix + "servers";
// all worker nodes
const static NodeID kWorkerGroup  = kGroupPrefix + "workers";
// kServerGroup + kWorkerGroup
const static NodeID kCompGroup    = kGroupPrefix + "comp_nodes";
// the nodes maintaining a replica of the key segment I own
const static NodeID kReplicaGroup = kGroupPrefix + "replicas";
// the owner nodes of the key segments this node backup
const static NodeID kOwnerGroup   = kGroupPrefix + "owners";
// all live nodes, including scheduler, workers, servers, unused nodes...
const static NodeID kLiveGroup    = kGroupPrefix + "lives";

// Executor maintain all remote nodes for a customer. It has its own thread to process
// received tasks.
class Executor {
 public:
  Executor(Customer& obj);
  ~Executor();

  // -- communication and synchronization --
  // see comments in customer.h
  int Submit(const MessagePtr& msg);
  void Accept(const MessagePtr& msg);
  void WaitSentReq(int timestamp, const NodeID& recver);
  void WaitRecvReq(int timestamp, const NodeID& sender);
  void FinishRecvReq(int timestamp, const NodeID& sender);

  // last received message
  MessagePtr activeMessage() { return active_msg_; }

 private:
  // Runs the DAG engine
  void Run() {
    while (!done_) {
      if (PickActiveMsg()) ProcessActiveMsg();
    }
  }
  // Returns true if a message with dependency satisfied is picked. Otherwise
  // will be blocked.
  bool PickActiveMsg();
  void ProcessActiveMsg();

  // Do management. Only thread-safe when run by "thread_".
  void ProcessControl(const MessagePtr& msg);
  void AddNode(const Node& node);
  void RemoveNode(const Node& node);
  void ReplaceNode(const Node& old_node, const Node& new_node);

  // -- received messages --
  std::list<MessagePtr> recv_msgs_;
  std::mutex msg_mu_;
  // the message is going to be processed or the last one be processed
  MessagePtr active_msg_;
  std::condition_variable dag_cond_;

  // -- remote nodes --
  std::mutex node_mu_;
  std::unordered_map<NodeID, RemoteNode> nodes_;

  RemoteNode* GetRNode(const NodeID& node_id) {
    auto it = nodes_.find(node_id);
    CHECK(it != nodes_.end()) << "node [" << node_id << "] doesn't exist";
    return &(it->second);
  }

  std::vector<NodeID> GroupIDs() {
   std::vector<NodeID> ids = {
     kServerGroup, kWorkerGroup, kCompGroup, kReplicaGroup, kOwnerGroup, kLiveGroup};
    return ids;
  }

  Customer& obj_;
  Postoffice& sys_;
  Node my_node_;
  int num_replicas_ = 0;
  bool done_ = false;
  std::thread* thread_ = nullptr;
};

// bool IsGroupNode(const NodeID& node_id) {
//   return node_id.compare(
//       0, std::max(node_id.size(), kGroupPrefix.size()), kGroupPrefix) == 0;
// }

} // namespace PS
