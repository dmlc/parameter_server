#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/yellow_pages.h"
#include "system/heartbeat_info.h"
#include "util/threadsafe_queue.h"
#include "dashboard.h"
namespace PS {

#define REGISTER_CUSTOMER(name, customer)                               \
  customer->setName(name);                                              \
  Postoffice::instance().yp().add(std::static_pointer_cast<Customer>(customer));

DECLARE_int32(num_servers);
DECLARE_int32(num_workers);
DECLARE_int32(num_unused);
DECLARE_int32(num_replicas);
DECLARE_string(node_file);
DECLARE_string(app);

class Postoffice {
 public:
  SINGLETON(Postoffice);
  ~Postoffice();
  // Run the system
  void run();
  // Queue a message into the sending buffer, which will be sent by the sending
  // thread.
  void queue(const MessagePtr& msg);
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
  Node& scheduler() { return yellow_pages_.van().scheduler(); }

  HeartbeatInfo& hb() { return heartbeat_info_; };

 private:
  DISALLOW_COPY_AND_ASSIGN(Postoffice);
  Postoffice() { }

  void manageApp(const Task& pt);
  void send();
  void recv();
  // heartbeat thread function
  void heartbeat();
  // monitor thread function only used by scheduler
  void monitor();

  // void addMyNode(const string& name, const Node& recver);

  string printDashboardTitle();
  string printHeartbeatReport(const string& node_id, const HeartbeatReport& report);

  std::mutex mutex_;
  bool done_ = false;

  std::promise<void> nodes_are_ready_;
  std::unique_ptr<std::thread> recving_;
  std::unique_ptr<std::thread> sending_;
  std::unique_ptr<std::thread> heartbeating_;
  std::unique_ptr<std::thread> monitoring_;
  threadsafe_queue<MessagePtr> sending_queue_;

  // yp_ should stay behind sending_queue_ so it will be destroied earlier
  YellowPages yellow_pages_;

  // heartbeat info for workers/servers
  HeartbeatInfo heartbeat_info_;
  Dashboard dashboard_;
};

} // namespace PS
