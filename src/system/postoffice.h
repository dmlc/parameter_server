#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/yellow_pages.h"
#include "util/threadsafe_queue.h"

namespace PS {

DECLARE_int32(num_servers);
DECLARE_int32(num_workers);
DECLARE_int32(num_unused);
DECLARE_int32(num_replicas);
DECLARE_string(node_file);

class Postoffice {
 public:
  SINGLETON(Postoffice);
  ~Postoffice();
  // Run the system
  void run();
  // Queue a message into the sending buffer, which will be sent by the sending
  // thread.
  void queue(const MessageCPtr& msg);
  // reply *task* from *recver* with *reply_msg*
  void reply(const NodeID& recver, const Task& task, const string& reply_msg = string());
  // reply message *msg* with protocal message *proto*
  template <class P> void replyProtocalMessage(const MessagePtr& msg, const P& proto) {
    string str; proto.SerializeToString(&str);
    reply(msg->sender, msg->task, str);
    msg->replied = true;
  }

  // add the nodes in _pt_ into the system
  void manageNode(const Task& pt);

  // accessors and mutators
  YellowPages& yp() { return yellow_pages_; }
  Node& myNode() { return yellow_pages_.van().myNode(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(Postoffice);
  Postoffice() { }

  void manageApp(const Task& pt);
  void send();
  void recv();
  void addMyNode(const string& name, const Node& recver);

  std::mutex mutex_;
  bool done_ = false;

  std::promise<void> nodes_are_ready_;
  std::unique_ptr<std::thread> recving_;
  std::unique_ptr<std::thread> sending_;
  threadsafe_queue<MessageCPtr> sending_queue_;

  // yp_ should stay behind sending_queue_ so it will be destroied earlier
  YellowPages yellow_pages_;
};

} // namespace PS
