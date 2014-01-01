#pragma once
#include "util/futurepool.h"
#include "util/common.h"
#include "util/mail.h"
#include "proto/express.pb.h"
#include "util/blocking_queue.h"
#include "system/van.h"
// #include "system/replica_manager.h"

namespace PS {
class Container;
class Inference;
class ReplicaManager;

class Postmaster;
typedef std::shared_future<string> ExpressReply;

// the postoffice accepts sending request from containers, and also notify
// containers if anything is received.
class Postoffice {
 public:
  SINGLETON(Postoffice);

  // init postmaster, start threads
  void Init();

  void Send(const Mail& mail) {
    package_sending_queue_.Put(mail);
  }
  void Send(Express cmd, ExpressReply* fut = NULL);

  void SetExpressReply(int express_label, const string& reply) {
    express_reply_.Set(express_label, reply);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(Postoffice);
  Postoffice() : inited_(false), postmaster_(NULL) { }
  bool inited_;

  // send/receive data
  void RecvPackage();
  void SendPackage();
  std::thread *send_package_;
  std::thread *recv_package_;
  BlockingQueue<Mail> package_sending_queue_;
  Van* package_van_;

  // send/receive commands
  void RecvExpress();
  void SendExpress();
  std::thread *send_express_;
  std::thread *recv_express_;
  BlockingQueue<Express> express_sending_queue_;
  Van* express_van_;
  FuturePool<string> express_reply_;
  int32 available_express_label_;

  Postmaster* postmaster_;
  // ReplicaManager* replica_manager_;
};

} // namespace PS
