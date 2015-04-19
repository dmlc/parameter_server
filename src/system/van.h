#pragma once
#include "util/common.h"
#include "system/proto/node.pb.h"
#include "system/message.h"
namespace PS {

/**
 * @brief Van sends (receives) packages to (from) a node The current
 * implementation uses ZeroMQ
 *
 */
class Van {
 public:
  Van() { }
  ~Van();

  void Init();

  void Disconnect(const Node&  node);
  bool Connect(const Node&  node);

  bool Send(Message* msg, size_t* send_bytes);
  bool Recv(Message* msg, size_t* recv_bytes);

  static Node ParseNode(const string& node_str);

  Node& my_node() { return my_node_; }
  Node& scheduler() { return scheduler_; };
 private:
  // bind to my port
  void Bind();

  static void FreeData(void *data, void *hint) {
    if (hint == NULL) {
      delete [] (char*)data;
    } else {
      delete (SArray<char>*)hint;
    }
  }

  bool IsScheduler() { return my_node_.role() == Node::SCHEDULER; }
  // for scheduler: monitor the liveness of all other nodes
  // for other nodes: monitor the liveness of the scheduler
  void Monitor();

  void *context_ = nullptr;
  void *receiver_ = nullptr;
  Node my_node_;
  Node scheduler_;
  std::unordered_map<NodeID, void *> senders_;

  DISALLOW_COPY_AND_ASSIGN(Van);

  // TODO move to postoffice::perf_monitor_
  // print statistic info
  void Statistic();
  std::unordered_map<NodeID, string> hostnames_;
  size_t sent_to_local_ = 0;
  size_t sent_to_others_ = 0;
  size_t received_from_local_ = 0;
  size_t received_from_others_ = 0;

  // for monitor
  std::unordered_map<int, NodeID> fd_to_nodeid_;
  std::mutex fd_to_nodeid_mu_;
  std::thread* monitor_thread_;

  // debug performance
  // double send_time_ = 0;
  // double recv_time_ = 0;
  // int num_call_ = 0;
};

} // namespace PS
