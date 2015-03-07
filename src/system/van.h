#pragma once
#include "util/common.h"
#include "system/proto/node.pb.h"
#include "system/message.h"
namespace PS {

// Van sends (receives) packages to (from) a node
// The current implementation uses ZeroMQ
class Van {
 public:
  Van() { }
  ~Van();

  void init(char*);

  void disconnect(const Node&  node);
  bool connect(const Node&  node);

  // check whether I could connect to a specified node
  bool connected(const Node& node);

  bool send(const MessagePtr& msg, size_t* send_bytes);
  bool recv(const MessagePtr& msg, size_t* recv_bytes);

  Node& myNode() { return my_node_; }
  Node& scheduler() { return scheduler_; };

 private:
  // bind to my port
  void bind();

  static Node parseNode(const string& config) {
    Node node; CHECK(TextFormat::ParseFromString(config, &node));
    if (!node.has_id()) node.set_id(address(node));
    return node;
  }
  static std::string address(const Node& node) {
    return node.hostname() + ":" + std::to_string(node.port());
  }
  // print statistic info
  void statistic();
  Node assembleMyNode();

  void *context_ = nullptr;
  void *receiver_ = nullptr;
  Node my_node_;
  Node scheduler_;
  std::unordered_map<NodeID, void *> senders_;
  std::unordered_map<NodeID, string> hostnames_;

  size_t sent_to_local_ = 0;
  size_t sent_to_others_ = 0;
  size_t received_from_local_ = 0;
  size_t received_from_others_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Van);
};

} // namespace PS
