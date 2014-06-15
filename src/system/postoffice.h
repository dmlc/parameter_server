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

  void run();

  // std::vector<Node> requestNodes() { CHECK(false); }

  void queue(const Message& msg);

  YellowPages& yp() { return yp_; }
  Node& myNode() { return yp_.van().myNode(); }

  void reply(
      const NodeID& recver, const Task& task, const string& reply_msg = string()) {
    if (!task.request()) return;
    Task tk;
    tk.set_customer(task.customer());
    tk.set_request(false);
    tk.set_type(Task::REPLY);
    if (!reply_msg.empty()) tk.set_msg(reply_msg);
    tk.set_time(task.time());

    Message re(tk);
    re.recver = recver;
    queue(re);
  }

  void reply(const Message& msg, const string& reply_msg = string());
  void manage_node(const Task& pt);
 private:
  DISALLOW_COPY_AND_ASSIGN(Postoffice);
  Postoffice() { }

  std::mutex mutex_;

  bool done_ = false;

  YellowPages yp_;

  void recv();

  // TODO fault tolerance
  void send();
  std::unique_ptr<std::thread> recving_;
  std::unique_ptr<std::thread> sending_;
  // std::thread *sending_;
  // std::thread *recving_;

  threadsafe_queue<Message> sending_queue_;

  void manage_app(const Task& pt);

};

} // namespace PS
