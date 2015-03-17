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
    Message* stop = new Message(); stop->terminate = true; Queue(stop);
    send_thread_->join();
  }
}

void Postoffice::Run(int* argc, char*** argv) {
  google::InitGoogleLogging((*argv)[0]);
  google::ParseCommandLineFlags(argc, argv, true);

  manager_.Init((*argv)[0]);

  if (FLAGS_report_interval > 0) {
    perf_monitor_.init(FLAGS_interface, manager_.van().my_node().hostname());
  }

  // start the I/O threads
  recv_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::Recv, this));
  send_thread_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::Send, this));

  manager_.Run();
}

// void Postoffice::Reply(const Message& msg, Task reply) {
//   const Task& task = msg.task;
//   if (!task.request()) return;
//   reply.set_request(false);
//   reply.set_control(task.control());
//   reply.set_time(task.time());
//   if (task.has_customer_id()) reply.set_customer_id(task.customer_id());
//   Message* reply_msg = new Message(reply);
//   reply_msg->recver = msg.sender;
//   Queue(reply_msg);
// }

// void Postoffice::reply(
//     const NodeID& recver, const Task& task, const string& reply_str) {
//   if (!task.request()) return;
//   Task reply_task;
//   reply_task.set_customer_id(task.customer_id());
//   reply_task.set_request(false);
//   reply_task.set_time(task.time());
//   reply_task.set_control(task.control());
//   if (!reply_str.empty()) reply_task.set_msg(reply_str);
//   MessagePtr reply_msg(new Message(reply_task));
//   reply_msg->recver = recver;
//   Queue(reply_msg);
// }

// void Postoffice::queue(const MessagePtr& msg) {
//   sending_queue_.push(msg);
// }


void Postoffice::Send() {
  Message* msg;
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg->terminate) break;
    size_t send_bytes = 0;
    manager_.van().Send(msg, &send_bytes);
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseOutBytes(send_bytes);
    }
    if (msg->task.request()) {
      // a request "msg" is safe to be deleted only if the response is received
      manager_.AddRequest(msg);
    } else {
      delete msg;
    }
  }
}

void Postoffice::Recv() {
  while (true) {
    // receive a message
    Message* msg = new Message();
    size_t recv_bytes = 0;
    CHECK(manager_.van().Recv(msg, &recv_bytes));
    if (FLAGS_report_interval > 0) {
      perf_monitor_.increaseInBytes(recv_bytes);
    }
    if (!msg->task.request()) manager_.AddResponse(msg);

    // process this message
    if (msg->task.control()) {
      bool ret = manager_.Process(msg);
      delete msg;
      if (!ret) break;
    } else {
      int id = msg->task.customer_id();
      // let the executor to delete "msg"
      manager_.customer(id)->executor()->Accept(msg);
    }
  }
}


} // namespace PS
