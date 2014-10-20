#pragma once
#include "util/common.h"
#include "system/message.h"
#include "system/yellow_pages.h"
#include "system/heartbeat_info.h"
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
  void addMyNode(const string& name, const Node& recver);

  string printDashboardTitle();
  string printHeartbeatReport(const string& node_id, const HeartbeatReport& report);

  std::mutex mutex_;
  bool done_ = false;

  std::promise<void> nodes_are_ready_;
  std::unique_ptr<std::thread> recving_;
  std::unique_ptr<std::thread> sending_;
  std::unique_ptr<std::thread> heartbeating_;
  std::unique_ptr<std::thread> monitoring_;
  threadsafe_queue<MessageCPtr> sending_queue_;

  // yp_ should stay behind sending_queue_ so it will be destroied earlier
  YellowPages yellow_pages_;

  // heartbeat info for workers/servers
  HeartbeatInfo heartbeat_info_;

  // record all heartbeat info by scheduler
  std::map<NodeID, HeartbeatReport,
    bool (*)(const NodeID& a, const NodeID& b)> dashboard_{
    [](const NodeID& a, const NodeID& b) -> bool {
      // lambda: split NodeID into primary segment and secondary segment
      auto splitNodeID = [] (const NodeID& in, string& primary, string& secondary) {
        size_t tailing_alpha_idx = in.find_last_not_of("0123456789");
        if (std::string::npos == tailing_alpha_idx) {
          primary = in;
          secondary = "";
        } else {
          primary = in.substr(0, tailing_alpha_idx + 1);
          secondary = in.substr(tailing_alpha_idx + 1);
        }
        return;
      };

      // split
      string a_primary, a_secondary;
      splitNodeID(a, a_primary, a_secondary);
      string b_primary, b_secondary;
      splitNodeID(b, b_primary, b_secondary);

      // compare
      if (a_primary != b_primary) {
        return a_primary < b_primary;
      } else {
        return std::stoul(a_secondary) < std::stoul(b_secondary);
      }
    }
  };
  // mutex protecting dashboard_
  std::mutex dashboard_mu_;
};

} // namespace PS
