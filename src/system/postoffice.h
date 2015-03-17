#pragma once
#include "util/common.h"
#include "system/message.h"
#include "util/threadsafe_queue.h"
#include "system/manager.h"
#include "system/heartbeat_info.h"
namespace PS {

class Postoffice {
 public:
  SINGLETON(Postoffice);
  ~Postoffice();

  void Run(int* argc, char***);
  void Stop() { manager_.Stop(); }

  // Queue a message into the sending buffer, which will be sent by the sending
  // thread. It is thread safe.
  //
  // "msg" will be DELETE by system when "msg" is sent successfully. so do NOT
  // delete "msg" before
  void Queue(Message* msg) { sending_queue_.push(msg); }

  // reply "msg" by "reply"
  // void Reply(const Message& msg, Task reply = Task());

  // Do not delete response
  void Reply(Message* request, Message* response = nullptr) { }

  Manager& manager() { return manager_; }
  HeartbeatInfo& pm() { return perf_monitor_; }

 private:
  Postoffice();
  void Send();
  void Recv();

  std::unique_ptr<std::thread> recv_thread_;
  std::unique_ptr<std::thread> send_thread_;
  ThreadsafeQueue<Message*> sending_queue_;

  Manager manager_;
  HeartbeatInfo perf_monitor_;
  DISALLOW_COPY_AND_ASSIGN(Postoffice);
};

  // deprecated
  // reply *task* from *recver* by *reply_str*. thread-safe
  // void reply(const NodeID& recver,
  //            const Task& task,
  //            const string& reply_str = string());
  // // reply *msg* with google protocbuf *proto*. thread-safe
  // template <class Proto>
  // void replyProtocalMessage(const MessagePtr& msg, const Proto& proto);
// template <class Proto>
// void Postoffice::replyProtocalMessage(
//     const MessagePtr& msg, const Proto& proto) {
//   string str; CHECK(proto.SerializeToString(&str));
//   reply(msg->sender, msg->task, str);
//   msg->replied = true;
// }

} // namespace PS
