#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util/blocking_queue.h"
#include "proto/header.pb.h"
#include "replica_manager.h"
#include "postoffice.h"
#include "postmaster.h"
#include "box/container.h"

namespace PS {

DEFINE_int32(replica_num, 3, "replica_num");
bool ReplicaManager::Init() {
  postoffice_ = Postoffice::Instance();
  postmaster_ = Postmaster::Instance();

  pthread_create(&thread_id_, NULL, WrapperThread, NULL);
  return true;
}

ReplicaManager::~ReplicaManager() {
  // for each fp
  // delete
  // delete replica_log_fp_;
}

void ReplicaManager::Put(const Mail &mail) {
  mails_received_.Put(mail);
}

// we should use some c++ feature to avoid this
void *ReplicaManager::WrapperThread(void *args) {
  ReplicaManager::Instance()->ReplicaManagerThread();
  return NULL;
}

void ReplicaManager::ReplicaManagerThread() {
  LOG(WARNING) << "replica thread start";
  while (ReplicaManager::NOTEXIT) {
    Mail mail = mails_received_.Take();
    const Header& flag = mail.flag();
    if (flag.type() == Header_Type_BACKUP) {
      LOG(WARNING) << "Header_Type_BACKUP";
      WriteToDisk(mail);
    } else if (flag.type() == Header_Type_NODE_RESCUE) {
      Rescue(mail);
    } else {
      // a normal message, then should backup this to the replica nodes
      Transfer(mail);
    }
  }
}

void ReplicaManager::Rescue(const Mail &mail) {
  // switch(command) {
  //   if ask to load temp data
  //     create a new vitural node
  //   if receive a transfered back up data
  //     write to disk
}

void ReplicaManager::Rescue(Container *ctr, int node_id, int replica_id) {
  ReplicaInfo info;
  info.container_name = ctr->name();
  info.node_id = node_id;
  info.replica_id = replica_id;
  ReadFromDisk(".", info, ctr);
}

void ReplicaManager::Transfer(const Mail &mail) {
  // mail.flag().set_type(SyncFlag_Type_BACKUP);
  // where can i get replica info for this key range
  for (int i = 0; i < FLAGS_replica_num; ++i) {
    map<uid_t, KeyRange> replica_map = postmaster_->GetReplicaTo(mail.flag().name(), i);
    std::vector<std::pair<uid_t, KeyRange>> replica_node_info;
    auto it = replica_map.begin();
    for (; it != replica_map.end(); ++it) {
      replica_node_info.push_back(make_pair(it->first, it->second));
    }
    for (int receiver = 0; receiver < (int)replica_node_info.size(); ++receiver) {
      RawArray key2, value2;
      key2 = Slice(mail.keys(), replica_node_info[receiver].second);
      value2 = Slice(mail.keys(), mail.vals(), key2);
      Header flag = mail.flag();
      flag.set_recver(replica_node_info[receiver].first);
      flag.set_type(Header_Type_BACKUP);
      flag.mutable_replica_info()->set_node_id(postmaster_->my_uid());
      flag.mutable_replica_info()->set_replica_id(replica_node_info[receiver].first);
      Mail mail2(flag, key2, value2);
      postoffice_->Send(mail2);
    }
  }
}

// would be async in the future
void ReplicaManager::WriteToDisk(Mail mail) {
  std::string buffer;
  CHECK(mail.Serialization(&buffer));
  ReplicaInfo info;
  info.container_name = mail.flag().name();
  info.node_id = mail.flag().replica_info().node_id();
  info.replica_id = mail.flag().replica_info().replica_id();
  const std::string file_name = info.ToString();
  auto it = replica_log_fp_.find(file_name);
  if (it == replica_log_fp_.end()) {
    LOG(WARNING) << "Creating Replica File: " << file_name;
    std::ofstream *fout = new std::ofstream(file_name);
    CHECK(fout != NULL) << "Create Replica File: " << file_name << " failed";
    replica_log_fp_[file_name] = fout;
  }
  (*replica_log_fp_[file_name]) << buffer;
}

void ReplicaManager::Read(std::string file_name, BlockingQueue<Mail> *queue) {
  int fd = open(file_name.c_str(), O_RDONLY);
  if (fd == -1) {
    LOG(ERROR) << "open " << file_name << " failed";
  }
  struct stat sb;
  if (fstat(fd, &sb) != 0) {
    LOG(ERROR) << "stat " << file_name << " failed";
  }
  size_t file_size = sb.st_size;
  char *data = (char *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  char *end_of_file = data + sb.st_size;
  while (data < end_of_file) {
    Mail mail;
    char *p = mail.ParseFromString(data, end_of_file);
    if (p == data) {
      LOG(WARNING) << "encounter incomplete data";
      break;
    }
    data = p;
    queue->Put(mail);
  }
  munmap(data, file_size);
}

void ReplicaManager::WriteToContainer(BlockingQueue<Mail> *queue, Container* ctr) {
  while(1) {
    Mail mail = queue->Take();
    if (mail.vals().size() == 0) {
      // after read all the mail, Reader thread add a empty mail
      // when read this empty mail, write thread can exit
      break;
    }
    // write mail to container
    ctr->Accept(mail);
    LOG(WARNING) << "get one mail, key size: " << mail.keys().size() << " val size: " << mail.vals().size();
  }
}

void ReplicaManager::ReadFromDisk(std::string dir, ReplicaInfo info, Container* ctr) {
  const std::string file_name = dir+info.ToString();
  BlockingQueue<Mail> mails_queue;
  LOG(WARNING) << "before read_thread";
  std::thread read_thread(&ReplicaManager::Read, this, file_name, &mails_queue);
  LOG(WARNING) << "after read_thread";
  std::thread write_thread(&ReplicaManager::WriteToContainer, this, &mails_queue, ctr);
  LOG(WARNING) << "after write_thread";
  read_thread.join();
  Mail mail;
  mails_queue.Put(mail);
  write_thread.join();
}

}
