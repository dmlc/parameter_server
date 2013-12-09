#pragma once
#include "util/common.h"
#include "util/mail.h"
#include "util/blocking_queue.h"
#include "system/van.h"
#include "system/postmaster.h"
#include "system/replica_manager.h"

namespace PS {
class Container;
class Inference;
class ReplicaManager;

class Postoffice;
// the postoffice accepts sending request from containers, and also notify
// containers if anything is received.
class Postoffice {
 public:
  SINGLETON(Postoffice);

  void Init();
  void Send(const Mail& mail) { sending_queue_.Put(mail); }

 private:
  DISALLOW_COPY_AND_ASSIGN(Postoffice);
  Postoffice() : inited_(false), postmaster_(NULL) { }
  bool inited_;

  // two postman threads, one for sending and one for receiving
  void RecvPostman();
  void SendPostman();
  std::thread *send_postman_;
  std::thread *recv_postman_;

  Van* van_;
  Postmaster* postmaster_;
  ReplicaManager* replica_manager_;
  BlockingQueue<Mail> sending_queue_;
};

} // namespace PS
