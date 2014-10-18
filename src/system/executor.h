#pragma once
#include "system/remote_node.h"
#include "system/message.h"

namespace PS {

// all server nodes
const static NodeID kServerGroup = Van::id("all_servers");
// all worker nodes
const static NodeID kWorkerGroup = Van::id("all_workers");
// kServerGroup + kWorkerGroup
const static NodeID kActiveGroup = Van::id("all_actives");
// the nodes maintaining a replica of the key segment I own
const static NodeID kReplicaGroup = Van::id("all_replicas");
// the owner nodes of the key segments this node backup
const static NodeID kOwnerGroup = Van::id("all_owners");
// all live nodes, including scheduler, workers, servers, unused nodes...
const static NodeID kLiveGroup = Van::id("all_lives");

// Maintain all remote nodes for a customer. It has its own thread to process
// received tasks.
class Executor {
 public:
  Executor(Customer& obj) : obj_(obj) {
    my_node_ = Postoffice::instance().myNode();
  }
  ~Executor() { }
  // not thread-safe, so call it before being used by other threads
  void init(const std::vector<Node>& nodes);

  // will be called by the customer's ctor, run the processing thread
  void run();
  // wake up the processing thread
  void notify() { Lock l(recved_msg_mu_); dag_cond_.notify_one(); }
  // stop the processing thread
  void stop() { done_ = true; notify(); }

  void finish(const MessagePtr& msg) {
    int t = msg->task.time();
    rnode(msg->sender)->finishIncomingTask(t);
  }

  // query nodes
  RNodePtr rnode(const NodeID& k) {
    Lock l(node_mu_);
    auto it = nodes_.find(k);
    // return an empty node
    if (it == nodes_.end()) return RNodePtr();
    return it->second;
  }
  RNodePtrList& group(const NodeID& k) {
    Lock l(node_mu_);
    auto it = node_groups_.find(k);
    CHECK(it != node_groups_.end()) << "unkonw node group: " << k;
    return it->second;
  }
  const KeyList& partition(const NodeID& k) {
    Lock l(node_mu_);
    auto it = node_key_partition_.find(k);
    CHECK(it != node_key_partition_.end()) << "unkonw node group: " << k;
    return it->second;
  }
  const Node& myNode() { return my_node_; }

  // NodeID myNodeID() { return my_node_.id(); }
  // bool isWorker() { return my_node_.role() == Node::WORKER; }
  // bool isServer() { return my_node_.role() == Node::SERVER; }

  // maintain nodes
  void add(const Node& node);
  // (somewhat) thread-safe, will called by postoffice's recving thread
  void replace(const Node& dead, const Node& live);

  // will be called by postoffice's receiving thread
  // or the thread call wk->submit
  void accept(const MessagePtr& msg);
  // last received message
  MessagePtr activeMessage() { return active_msg_; }
  string lastRecvReply();

  Customer& obj() { return obj_; }
 private:
  Customer& obj_;
  // Temporal buffer for received messages
  std::list<MessagePtr> recved_msgs_;
  std::mutex recved_msg_mu_;
  // the message is going to be processed or is the last one be processed
  MessagePtr active_msg_;
  std::condition_variable dag_cond_;
  std::unordered_map<NodeID, RNodePtr> nodes_;
  std::unordered_map<NodeID, RNodePtrList> node_groups_;
  std::unordered_map<NodeID, KeyList> node_key_partition_;
  Node my_node_;
  std::mutex node_mu_;
  bool done_ = false;
};


} // namespace PS
