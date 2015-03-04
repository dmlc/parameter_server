#include "system/postoffice.h"
#include "system/customer.h"
#include "system/app.h"
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

  if (FLAGS_report_interval > 0) {
    perf_monitor_.init(FLAGS_interface, manager_.van().myNode().hostname());
  }
  manager_.init();

  // start the I/O threads
  recv_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::recv, this));
  send_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::send, this));

  manager_.run();
}


void Postoffice::reply(
    const NodeID& recver, const Task& task, const string& reply_str) {
  if (!task.request()) return;
  Task reply_task;
  reply_task.set_customer(task.customer());
  reply_task.set_request(false);
  reply_task.set_time(task.time());
  reply_task.set_type(task.type());
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
    reply_task.set_customer(msg->task.customer());
    reply_task.set_request(false);
    reply_task.set_type(msg->task.type());
    reply_task.set_time(msg->task.time());
    MessagePtr reply_msg(new Message(reply_task));
    reply_msg->sender = msg->recver;
    reply_msg->recver = msg->sender;
    manager_.customer(msg->task.customer())->exec().accept(reply_msg);
  }
}


void Postoffice::send() {
  MessagePtr msg;
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg->terminate) break;
    size_t send_bytes = 0;
    Status stat = manager_.van().send(msg, &send_bytes);
    LOG_IF(INFO, !stat.ok())
        << "failed to send message to " << msg->sender << ". error: "
        << stat.ToString() << ". msg: " << *msg;
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseOutBytes(send_bytes);
    }
  }
}

void Postoffice::recv() {
  while (true) {
    // receive a message
    MessagePtr msg(new Message());
    size_t recv_bytes = 0;
    Status stat = manager_.van().recv(msg, &recv_bytes);
    CHECK(stat.ok()) << "failed to recv a message. " << stat.ToString();
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseInBytes(recv_bytes);
    }

    // process this message
    auto task_type = msg->task.type();
    if (task_type == Task::CALL_CUSTOMER) {
      const auto& id = msg->task.customer();
      Customer* obj = manager_.customer(id);
      CHECK(obj) << "customer [" << id << "] doesn't exist";
      obj->exec().accept(msg);
    } else if (task_type == Task::CONTROL) {
      if (!manager_.process(msg)) break;
    }
  }
}


} // namespace PS
