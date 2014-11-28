#pragma once
#include "util/common.h"
#include "util/status.h"
#include "proto/node.pb.h"
#include "system/message.h"

namespace PS {

DECLARE_string(my_node);
// Van sends (receives) packages to (from) a node
// The current implementation uses ZeroMQ
class Van {
 public:
  Van() : context_(nullptr), receiver_(nullptr) {}
  ~Van() { }
  void init();
  void destroy();

  Status connect(const Node&  node);

  // check whether I could connect to a specified node
  bool connected(const Node& node);

  // Status send(const MessagePtr& msg);
  Status send(const MessagePtr& msg, size_t* send_bytes);
  Status recv(const MessagePtr& msg, size_t* recv_bytes);

  Node& myNode() { return my_node_; }
  Node& scheduler() { return scheduler_; };

  // utility functions for node
  static Node parseNode(const string& config) {
    Node node; CHECK(TextFormat::ParseFromString(config, &node));
    if (!node.has_id()) node.set_id(id(address(node)));
    return node;
  }

  static std::string address(const Node& node) {
    return node.hostname() + ":" + std::to_string(node.port());
  }
  static const NodeID id(const std::string& name) {
    return name;
  }

  // print statistic info
  void statistic();

 private:
  DISALLOW_COPY_AND_ASSIGN(Van);
  // bind to my port
  void bind();
  void *context_;
  void *receiver_;
  Node my_node_;
  Node scheduler_;
  std::mutex mu_;
  std::unordered_map<NodeID, void *> senders_;
  std::unordered_map<NodeID, string> hostnames_;

  size_t sent_to_local_ = 0;
  size_t sent_to_others_ = 0;
  size_t received_from_local_ = 0;
  size_t received_from_others_ = 0;

  std::ofstream debug_out_;

  Node assembleMyNode();
};

} // namespace PS
