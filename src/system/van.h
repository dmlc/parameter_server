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

  Status connect(Node const& node);

  // check whether I could connect to a specified node
  Status connectivity(const string &node_id);

  // Status send(const MessagePtr& msg);
  Status send(const MessageCPtr& msg, size_t& send_bytes);
  Status recv(const MessagePtr& msg, size_t& recv_bytes);

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
  std::map<NodeID, void *> senders_;

  size_t data_sent_ = 0;
  size_t data_received_ = 0;
  std::ofstream debug_out_;
  int num_retries_ = 0;
  // std::ostream& debug_out_;

  Node assembleMyNode();
};

} // namespace PS
