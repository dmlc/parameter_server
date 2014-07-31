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

  // queue a message into the sending buffer, which will be sent by the sending
  // thread.
  void queue(const Message& msg);

  // send the reply message for the _task_ from _recver_
  void reply(const NodeID& recver,
             const Task& task,
             const string& reply_msg = string());

  void reply(const Message& msg, const string& reply_msg = string());

  // add the nodes in _pt_ into the system
  void manageNode(const Task& pt);

  // accessors and mutators
  YellowPages& yp() { return yp_; }
  Node& myNode() { return yp_.van().myNode(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(Postoffice);
  Postoffice() { }

  void manage_app(const Task& pt);
  void send();
  void recv();

  std::mutex mutex_;
  bool done_ = false;

  std::unique_ptr<std::thread> recving_;
  std::unique_ptr<std::thread> sending_;

  threadsafe_queue<Message> sending_queue_;

  // yp_ should stay behind sending_queue_ so it will be destroied earlier
  YellowPages yp_;
};

} // namespace PS
