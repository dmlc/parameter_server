#pragma once
#include "system/remote_node.h"
#include "system/message.h"

namespace PS {

// all server nodes
const static NodeID kServerGroup = "all_servers";
// all worker nodes
const static NodeID kWorkerGroup = "all_workers";
// kServerGroup + kWorkerGroup
const static NodeID kCompGroup = "all_comp_nodes";
// the nodes maintaining a replica of the key segment I own
const static NodeID kReplicaGroup = "all_replicas";
// the owner nodes of the key segments this node backup
const static NodeID kOwnerGroup = "all_owners";
// all live nodes, including scheduler, workers, servers, unused nodes...
const static NodeID kLiveGroup = "all_lives";

typedef std::vector<Node> NodeList;

// Maintain all remote nodes for a customer. It has its own thread to process
// received tasks.
class Executor {
 public:
  Executor(Customer& obj);
  ~Executor();

  // wake up the processing thread
  void notify() { Lock l(recved_msg_mu_); dag_cond_.notify_one(); }

  // mark this message as finshed in executor
  void finish(const MessagePtr& msg);

  // stop the processing thread
  void stop();

  // accessors
  RNode* rnode(const NodeID& k);
  std::vector<RNode*>& group(const NodeID& k);
  const std::vector<Range<Key>>& keyRanges(const NodeID& k);
  const Node& myNode() { return my_node_; }

  void addNode(const Node& node);
  void removeNode(const Node& node);
  void replaceNode(const Node& old_node, const Node& new_node);

  // will be called by postoffice's receiving thread
  // or the thread call wk->submit
  void accept(const MessagePtr& msg);

  // last received message
  MessagePtr activeMessage() { return active_msg_; }

  Customer& obj() { return obj_; }
 private:
  void run();

  Customer& obj_;
  // Temporal buffer for received messages
  std::list<MessagePtr> recved_msgs_;
  std::mutex recved_msg_mu_;
  // the message is going to be processed or is the last one be processed
  MessagePtr active_msg_;
  std::condition_variable dag_cond_;

  std::vector<NodeID> groupIDs() {
   std::vector<NodeID> ids = {
     kServerGroup, kWorkerGroup, kCompGroup,
     kReplicaGroup, kOwnerGroup, kLiveGroup};
    return ids;
  }

  struct NodeInfo {
    NodeInfo() { };
    ~NodeInfo() { delete node; }

    void addSubNode(RNode* s);
    void removeSubNode(RNode* s);

    RNode* node = NULL;
    // sub_nodes = {node} or all nodes in this node group
    std::vector<RNode*> sub_nodes;
    std::vector<Range<Key>> key_ranges;
  };

  std::unordered_map<NodeID, NodeInfo> nodes_;

  Node my_node_;
  std::mutex node_mu_;
  bool done_ = false;
  std::unique_ptr<std::thread> thread_;
};


} // namespace PS
