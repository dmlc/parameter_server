#include "system/address_book.h"

namespace PS {

DEFINE_int32(my_rank, 0, "my rank id, continous integer from 0");
DEFINE_string(my_type, "server", "type of my node, client, or server");
DEFINE_int32(num_server, 1, "number of servers");
DEFINE_int32(num_client, 1, "number of clients");
DEFINE_string(server_address,
              "tcp://localhost:7100,tcp://localhost:9102,tcp://localhost:7004,tcp://localhost:7006,tcp://localhost:7008,tcp://localhost:7010",
              "address of servers");
DEFINE_string(client_address,
              "tcp://localhost:6050,tcp://localhost:6012,tcp://localhost:6004,tcp://localhost:6006,tcp://localhost:6008,tcp://localhost:6010",
              "address of clients");

string AddressBook::DebugString() {
  std::stringstream ss;
  ss << "#client: " << FLAGS_num_client
     << ", #server: " << FLAGS_num_server
     << ", my rank: " << FLAGS_my_rank
     << ", my type: " << FLAGS_my_type;
  return ss.str();
}

void AddressBook::InitNodes() {
  num_server_ = FLAGS_num_server;
  num_client_ = FLAGS_num_client;
  std::vector<string> s_addr = split(FLAGS_server_address, ',');
  std::vector<string> c_addr = split(FLAGS_client_address, ',');
  CHECK_GE(s_addr.size(), num_server_)
      << "#address in " << FLAGS_server_address << " is less than num_server";
  CHECK_GE(c_addr.size(), num_client_)
      << "#address in " << FLAGS_client_address << " is less than num_client";
  Node node;
  for (size_t i = 0; i < num_server_; ++i) {
    // if (IamBackupProcess()) {
    //   if (i == FLAGS_failed_node_id)
    //     continue;
    // }
    // network address
    string mail_addr = s_addr[i];
    std::vector<string> part = split(mail_addr, ':');
    // use data_port + 1 to send cmd
    int port = std::stoi(part.back());
    port ++;
    string cmd_addr;
    for (size_t j = 0; j < part.size(); j++) {
      if (j != part.size() - 1 )
        cmd_addr += (part[j] + ':');
      else
        cmd_addr += std::to_string(port);
    }
    node.Init(Node::kTypeServer, i, mail_addr, cmd_addr);
    // insert into node groups
    uid_t uid = node.uid();
    all_[uid] = node;
    group_.servers()->push_back(uid);
    group_.all()->push_back(uid);
    if (i==0) group_.set_root(uid);
  }
  // client nodes
  for (size_t i = 0; i < num_client_; ++i) {
    string mail_addr = c_addr[i];
    std::vector<string> part = split(mail_addr, ':');
    // use data_port + 1 to send cmd
    int port = std::stoi(part.back());
    port ++;
    string cmd_addr;
    for (size_t j = 0; j < part.size(); j++) {
      if (j != part.size() - 1)
        cmd_addr += (part[j] + ':');
      else
        cmd_addr += std::to_string(port);
    }
    node.Init(Node::kTypeClient, i, mail_addr, cmd_addr);
    // insert into node groups
    uid_t uid = node.uid();
    all_[uid] = node;
    group_.clients()->push_back(uid);
    group_.all()->push_back(uid);
  }

  my_uid_ = Node::GetUid(FLAGS_my_type, FLAGS_my_rank);
  CHECK(all_.find(my_uid_) != all_.end())
      << "there is no my_node [" << my_uid_ << "] info. "
      << DebugString();
}

void AddressBook::InitVans() {
  // data connections
  package_van_ = new Van();
  CHECK(package_van_->Init());
  CHECK(package_van_->Bind(my_node(), 0));

  if (my_node().is_client()) {
    for (auto id : *group_.servers())
      CHECK(package_van_->Connect(all_[id], 0));
  } else {
    // connect to all
    for (auto id : *group_.all()) {
      if (id != my_uid())  // TODO no if
        CHECK(package_van_->Connect(all_[id], 0));
    }
  }
  // control connections
  express_van_ = new Van();
  CHECK(express_van_->Init());
  CHECK(express_van_->Bind(my_node(), 1));
  if (IamRoot()) {
    for (auto id : *group_.all()) {
      CHECK(express_van_->Connect(all_[id], 1));
    }
  } else {
    CHECK(express_van_->Connect(root(), 1));
  }
}

} // namespace PS
