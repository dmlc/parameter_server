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

// Executor maintain all remote nodes for a customer. It has its own thread to process
// received tasks.
class Executor {
 public:
  Executor(Customer& obj);
  ~Executor();

  // Submits a request message into a remote node. See comments in customer.h
  int Submit(const MessagePtr& msg) {

  }


  // mark this message as finshed in executor
  void finish(const MessagePtr& msg);

  // stop the processing thread
  void stop();

  // accessors
  RNode* rnode(const NodeID& k);
  std::vector<RNode*>& group(const NodeID& k);
  const std::vector<Range<Key>>& keyRanges(const NodeID& k);
  const Node& myNode() { return my_node_; }


  // Accepts a received message from Postoffice. Thread-safe.
  void Accept(const MessagePtr& msg);

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
  void Manage(const MessagePtr& msg);
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





  void run();
  Customer& obj_;
  // Temporal buffer for received messages

  std::vector<NodeID> groupIDs() {
   std::vector<NodeID> ids = {
     kServerGroup, kWorkerGroup, kCompGroup,
     kReplicaGroup, kOwnerGroup, kLiveGroup};
    return ids;
  }

  struct NodeInfo {
    NodeInfo() { };
    ~NodeInfo() { delete node; }

    void clear() { sub_nodes.clear(); key_ranges.clear(); }
    void addSubNode(RNode* s);
    void removeSubNode(RNode* s);

    RNode* node = NULL;
    // sub_nodes = {node} or all nodes in this node group. they are ordered
    // according to the key range
    std::vector<RNode*> sub_nodes;
    std::vector<Range<Key>> key_ranges;
  };

  Node my_node_;
  int num_replicas_ = 0;
  bool done_ = false;
  std::thread* thread_ = nullptr;
};


} // namespace PS
