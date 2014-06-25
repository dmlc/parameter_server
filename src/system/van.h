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
  // void Init(const Node& my_node);
  void init();
  void destroy();

  // connect to a node
  Status connect(Node const& node);

  Status send(const Message& msg);
  Status recv(Message* msg);

  Node& myNode() { return my_node_; }

  // utility functions for node
  static Node parseNode(const string& config) {
    Node node;
    CHECK(TextFormat::ParseFromString(config, &node));
    if (!node.has_id())
      node.set_id(id(address(node)));
    return node;
  }

  static std::string address(const Node& node) {
    return node.hostname() + ":" + std::to_string(node.port());
  }
  static const NodeID id(const std::string& name) {
    return name;
    // std::hash<std::string> fn;
    // return fn(name);
  }
  // static NodeID id(const Node& node) {
  //   return id(address(node));
  // }

  // print statistic info
  void statistic();

 private:
  DISALLOW_COPY_AND_ASSIGN(Van);
  // bind to my port
  void bind();
  void *context_;
  void *receiver_;
  Node my_node_;
  std::mutex mu_;
  std::map<NodeID, void *> senders_;

  size_t send_head_ = 0;
  size_t send_uncompressed_ = 0;
  size_t send_compressed_ = 0;

  size_t recv_head_ = 0;
  size_t recv_uncompressed_ = 0;
  size_t recv_compressed_ = 0;

  std::ofstream debug_out_;
};

} // namespace PS
