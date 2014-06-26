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

class Executor {
 public:
  Executor(Customer& obj) : obj_(obj) { }
  ~Executor() { }

  // not thread-safe, so call it before being used by other threads
  void init(const std::vector<Node>& nodes);

  // (somewhat) thread-safe, will called by postoffice's recving thread
  void replace(const Node& dead, const Node& live);

  // will be called by postoffice's receiving thread
  // or the thread call wk->submit
  void accept(const Message& msg);


  // will be called by customer ctor,
  void run();
  void stop() { done_ = true; notify(); }

  void notify() { Lock l(recved_msg_mu_); dag_cond_.notify_one(); }

  void finish(const Message& msg) {
    int t = msg.task.time();
    rnode(msg.sender)->finishIncomingTask(t);
  }

  RNodePtr rnode(const NodeID& k) {
    Lock l(node_mu_);
    auto it = nodes_.find(k);
    if (it == nodes_.end()) {
      LL << my_node_.id() << " look for empty " << k;
      return RNodePtr();
    }
    // CHECK(it != nodes_.end()) << "unkonw node: " << k << " i'm " << my_node_.uid();
    return it->second;
  }

  RNodePtrList& group(const NodeID& k) {
    Lock l(node_mu_);
    auto it = node_groups_.find(k);
    CHECK(it != node_groups_.end()) << "unkonw node group: " << k;
    return it->second;
  }

  Keys& partition(const NodeID& k) {
    Lock l(node_mu_);
    auto it = node_key_partition_.find(k);
    CHECK(it != node_key_partition_.end()) << "unkonw node group: " << k;
    return it->second;
  }

  Customer& obj() { return obj_; }
  Node& myNode() { return my_node_; }
  bool isWorker() { return my_node_.role() == Node::CLIENT; }
  bool isServer() { return my_node_.role() == Node::SERVER; }

  Message& activeMessage() { return active_msg_; }
  string lastRecvReply();
  // NodeID activeOriginalReceiver;

  // return a more meaningful name for debug use
  // string dbname(const NodeID& uid);


 private:

  void add(const Node& node);
  // void remove(const Node& node);

  Customer& obj_;

  // Temporal buffer for received messages
  std::list<Message> recved_msgs_;
  std::mutex recved_msg_mu_;

  std::condition_variable dag_cond_;

  // the message is going to be processed or is the last one be processed
  Message active_msg_;

  std::unordered_map<NodeID, RNodePtr> nodes_;
  std::unordered_map<NodeID, RNodePtrList> node_groups_;
  std::unordered_map<NodeID, Keys> node_key_partition_;
  std::mutex node_mu_;

  Node my_node_;

  bool done_ = false;
};


} // namespace PS
