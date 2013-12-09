#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "util/common.h"
#include "util/mail.h"
#include "util/blocking_queue.h"
#include "box/container.h"

namespace PS {

class Postoffice;
class Postmaster;
class Container;

class ReplicaManager {
 public:
  SINGLETON(ReplicaManager);
  static const constexpr char *LOG_FILE_PREFIX = "ps.replica";
  class ReplicaInfo {
   public:
    std::string container_name;
    int node_id; // which node we back up
    int replica_id; // which replica, we have three replica for each data
    std::string ToString() {
      std::stringstream ss;
      ss << LOG_FILE_PREFIX << "_" << container_name << "_" << node_id << "_" << replica_id;
      return ss.str();
    }
  };
  ~ReplicaManager();
  
  bool Init();
  // add mail to mails_to_transfer_ queue
  void Transfer(const Mail &mail);
  // add mail to mails_received_ queue
  void Accept(const Mail &mail);

  void ReplicaManagerThread();

  void Put(const Mail &mail);

  void WriteToDisk(Mail mail);

  void Rescue(const Mail &mail);
  void Rescue(Container *ctr, int node_id, int replica_id);
  
  void Read(std::string file_name, BlockingQueue<Mail> *queue);
  
  void ReadFromDisk(std::string dir, ReplicaInfo info, Container* ctr);

  void WriteToContainer(BlockingQueue<Mail> *queue, Container* ctr);

  static void *WrapperThread(void *);
 private:
  const bool NOTEXIT = true;
  BlockingQueue<Mail> mails_received_;
  Postoffice *postoffice_;
  Postmaster *postmaster_;
  std::map<std::string, std::ofstream *> replica_log_fp_;
  pthread_t thread_id_;
};

}
