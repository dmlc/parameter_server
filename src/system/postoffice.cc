#include "system/postoffice.h"
#include "system/customer.h"
#include "util/file.h"
// #include <omp.h>

namespace PS {

DEFINE_int32(report_interval, 0,
  "Servers/Workers report running status to the scheduler "
  "in every report_interval seconds. "
  "default: 0; if set to 0, heartbeat is disabled");

DECLARE_string(interface);

Postoffice::Postoffice() { }

Postoffice::~Postoffice() {
  if (recv_thread_) recv_thread_->join();
  if (send_thread_) {
    MessagePtr stop(new Message()); stop->terminate = true; queue(stop);
    send_thread_->join();
  }
}

void Postoffice::run(int* argc, char*** argv) {
  google::InitGoogleLogging((*argv)[0]);
  google::ParseCommandLineFlags(argc, argv, true);

  manager_.init((*argv)[0]);

  if (FLAGS_report_interval > 0) {
    perf_monitor_.init(FLAGS_interface, manager_.van().myNode().hostname());
  }

  // start the I/O threads
  recv_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::recv, this));
  send_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::send, this));

  manager_.run();
}

void Postoffice::reply(const MessagePtr& msg, Task reply) {
  const Task& task = msg->task;
  if (!task.request()) return;
  reply.set_request(false);
  reply.set_control(task.control());
  reply.set_time(task.time());
  if (task.has_customer_id()) reply.set_customer_id(task.customer_id());
  MessagePtr reply_msg(new Message(reply));
  reply_msg->recver = msg->sender;
  queue(reply_msg);
}

void Postoffice::reply(
    const NodeID& recver, const Task& task, const string& reply_str) {
  if (!task.request()) return;
  Task reply_task;
  reply_task.set_customer_id(task.customer_id());
  reply_task.set_request(false);
  reply_task.set_time(task.time());
  reply_task.set_control(task.control());
  if (!reply_str.empty()) reply_task.set_msg(reply_str);
  MessagePtr reply_msg(new Message(reply_task));
  reply_msg->recver = recver;
  queue(reply_msg);
}

void Postoffice::queue(const MessagePtr& msg) {
  if (msg->valid) {
    sending_queue_.push(msg);
  } else {
    // do not send, fake a reply mesage
    Task reply_task;
    reply_task.set_customer_id(msg->task.customer_id());
    reply_task.set_request(false);
    reply_task.set_control(msg->task.control());
    reply_task.set_time(msg->task.time());
    MessagePtr reply_msg(new Message(reply_task));
    reply_msg->sender = msg->recver;
    reply_msg->recver = msg->sender;
    manager_.customer(msg->task.customer_id())->exec().accept(reply_msg);
  }
}


void Postoffice::send() {
  MessagePtr msg;
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg->terminate) break;
    size_t send_bytes = 0;
    manager_.van().send(msg, &send_bytes);
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseOutBytes(send_bytes);
    }
    manager_.addPendingMsg(msg);
  }
}

void Postoffice::recv() {
  while (true) {
    // receive a message
    MessagePtr msg(new Message());
    size_t recv_bytes = 0;
    CHECK(manager_.van().recv(msg, &recv_bytes));
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseInBytes(recv_bytes);
    }
    manager_.removePendingMsg(msg);

    // process this message
    if (msg->task.control()) {
      if (!manager_.process(msg)) break;
    } else {
      int id = msg->task.customer_id();
      manager_.customer(id)->exec().accept(msg);
    }
  }
}


} // namespace PS
