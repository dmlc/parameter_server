#include "system/postmaster.h"
#include "box/container.h"
#include "algo/inference.h"
#include "util/status.h"
namespace PS {

// input parameters
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
// input parameters -- number of virtual node per physical node
DEFINE_int32(num_vnode, 1, "number of virtual node per physical node");
DEFINE_int32(wait_seconds, 1, "set the threshold for waiting back, if > threshold, then node is dead");
DEFINE_int32(ping_period, 10, "ping the servers every ping_period seconds");
DEFINE_bool(is_backup_process, false, "used to denote whether a node is a back up node");
DEFINE_int32(failed_node_id, 100, "which node fails, used to initialize a back up process");
DEFINE_int32(global_feature_num, 10, "num of global feature");
DEFINE_int32(local_feature_num, 10, "num of local feature");


void Postmaster::Init() {
  // create a postmaster thread
  // wait for command from cluster master
  // LL << "flags:" << FLAGS_my_rank << FLAGS_num_server << FLAGS_num_clietn;
  is_backup_process_ = FLAGS_is_backup_process;
  size_t num_server_ = FLAGS_num_server;
  size_t num_client_ = FLAGS_num_client;
  std::vector<string> s_addr = split(FLAGS_server_address, ',');
  std::vector<string> c_addr = split(FLAGS_client_address, ',');
  CHECK_GE(s_addr.size(), num_server_)
      << "#address in " << FLAGS_server_address << " is less than num_server";
  CHECK_GE(c_addr.size(), num_client_)
      << "#address in " << FLAGS_client_address << " is less than num_client";
  // init all availabe nodes
  Node node;
  for (size_t i = 0; i < num_server_; ++i) {
    if (IamBackupProcess()) {
      if (i == FLAGS_failed_node_id)
        continue;
    }
    string mail_addr = s_addr[i];
    std::vector<string> part = split(mail_addr, ':');
    int port = std::stoi(part.back());
    // port + 1 used to send cmd
    port ++;
    string cmd_addr;
    for (size_t j = 0; j < part.size(); j++) {
      if (j != part.size() - 1 )
        cmd_addr += (part[j] + ':');
      else
        cmd_addr += std::to_string(port);
    }
    node.Init(Node::kTypeServer, i, mail_addr, cmd_addr);
    uid_t uid = node.uid();
    all_[uid] = node;
    group_.servers()->push_back(uid);
    group_.all()->push_back(uid);
    if (i==0) group_.set_root(uid);
  }
  for (size_t i = 0; i < num_client_; ++i) {
    string mail_addr = c_addr[i];
    std::vector<string> part = split(mail_addr, ':');
    int port = std::stoi(part.back());
    // port + 1 used to send cmd
    port ++;
    string cmd_addr;
    for (size_t j = 0; j < part.size(); j++) {
      if (j != part.size() - 1)
        cmd_addr += (part[j] + ':');
      else
        cmd_addr += std::to_string(port);
    }

    node.Init(Node::kTypeClient, i, mail_addr, cmd_addr);
    uid_t uid = node.uid();
    all_[uid] = node;
    group_.clients()->push_back(uid);
    group_.all()->push_back(uid);
  }
  // my node
  my_uid_ = Node::GetUid(FLAGS_my_type, FLAGS_my_rank);
  CHECK(all_.find(my_uid_) != all_.end()) << "invalid rank: " << my_uid_;
  LL << "I'm" << my_node().ToString();

  // init connections for mail
  van_ = new Van();
  CHECK(van_->Init());
  CHECK(van_->Bind(my_node(), 0));
  // init connections for cmd
  cmd_van_ = new Van();
  CHECK(cmd_van_->Init());
  CHECK(cmd_van_->Bind(my_node(), 1));

  // connet to all (not necessary, but the class item implemeted allreduce)
  // mail connect
  if (my_node().is_client()) {
    for (auto id : *group_.servers())
      CHECK(van_->Connect(all_[id], 0));
  }
  else {
    for (auto id : *group_.all())
    {
      if (id != my_uid())
        CHECK(van_->Connect(all_[id], 0));
    }
  }

  // cmd connect
  if (my_uid_ == 0) {
    for (auto id : *group_.all()) {
      if (id != my_uid_)
        CHECK(cmd_van_->Connect(all_[id], 1));
    }
  } else {
    // connect to the master
    CHECK(cmd_van_->Connect(all_[0], 1));
  }

  // create DHT instance to manage the key-range distributation

  send_thread_ = new std::thread(&Postmaster::send_cmd, this);
  receive_thread_ = new std::thread(&Postmaster::receive_cmd, this);

  // dht_ needs to be deleted in deconstructor
  // set the number of virtual nodes per physical node
  size_t num_vnode = FLAGS_num_vnode;
  dht_ = new DHT(num_vnode);
}

void Postmaster::Register(Inference *ifr, DataRange whole) {
  // a simple version, give ifr all client nodes, and evenly divide the training samples
  NodeGroup group;
  size_t num = group_.clients()->size();
  for (size_t i = 0; i < num; ++i) {
    uid_t id = group_.clients()->at(i);
    Workload wl(id, KeyRange(), whole.EvenDivide(num, i));
    workloads_[make_pair(ifr->name(), id)] = wl;
    group.clients()->push_back(id);    group.all()->push_back(id);
  }
  nodegroups_[ifr->name()] = group;
}

KeyRange Postmaster::Register(Container *ctr, KeyRange whole, Inference *ifr) {
  // first store its pointer
  containers_[ctr->name()] = ctr;

  LL << "register is called";

  if (my_uid_ == 0)
  {
    dhtinfo_ = dht_->AssignNodes(ctr, whole, group_, all_);
    MasterAssignNodes(ctr, whole);
  } else {
    SlaveAssignNodes(ctr, whole);
  }

  uid_t id = NodeGroup::kServers;
  Workload wl(id, whole);
  workloads_[make_pair(ctr->name(), id)] = wl;

  return workloads_[make_pair(ctr->name(), my_uid())].key_range();

}

void Postmaster::MasterAssignNodes(Container *ctr, KeyRange whole) {
  LL << "master assign nodes";
  MapServerKeyRange(ctr, whole);
  int num = group_.all()->size();
  // set the sender, then send all them to clients and servers
  for (size_t j = 0; j < num; ++j)
    SendKeyRange(ctr, group_.all()->at(j));
  LL << "finished sending key ranges of container " << ctr->name();
  SendReplicaInfo(ctr);
}

void Postmaster::MapServerKeyRange(Container *ctr, KeyRange whole) {
  // a simple version here, assign all nodes to this container
  containers_[ctr->name()] = ctr;
  NodeGroup group;
  // divide the key range evenly to server nodes,
  size_t num = group_.servers()->size();
  for (size_t i = 0; i < num; ++i) {
    uid_t id = group_.servers()->at(i);
    // KeyRange kr = whole.EvenDivide(num, i, 100);
    // dht keyrange
    KeyRange kr = dhtinfo_.workloads_[make_pair(ctr->name(), id)].key_range();
    if (!kr.Valid()) continue;
    Workload wl(id, kr);
    group.servers()->push_back(id);
    group.all()->push_back(id);
    workloads_[make_pair(ctr->name(), id)] = wl;
  }

  // divde data evenly to client nodes
  //
  num = group_.clients()->size();
  for (size_t i = 0; i < num; ++i) {
    uid_t id = group_.clients()->at(i);
    Workload wl(id, whole, DataRange());
    workloads_[make_pair(ctr->name(), id)] = wl;
    group.clients()->push_back(id);
    group.all()->push_back(id);
  }
  // set a root, it is used for doing allreduce
  group.set_root(Node::GetUid("server", 0));
  nodegroups_[ctr->name()] = group;
}


void Postmaster::SendKeyRange(Container *ctr, uid_t id) {
  NodeGroup group = nodegroups_[ctr->name()];
  NodeManagementInfo mgt_info;
  mgt_info.set_name(ctr->name());
  mgt_info.set_sender(my_uid_);
  mgt_info.set_command_id(NodeManagementInfo::CLIENT_MSG_KEY_RANGE);

  size_t num = group.servers()->size();
  for (size_t i = 0; i < num; ++i)
  {
    MsgKeyRange* mkr = mgt_info.add_msg_key_ranges();
    mkr->set_node_id(group.servers()->at(i));
    Workload wl = workloads_[make_pair(ctr->name(), group.servers()->at(i))];
    mkr->set_key_start(wl.key_range().start());
    mkr->set_key_end(wl.key_range().end());
    LL << "key start:" << wl.key_range().start() << " key end: "<< wl.key_range().end();
  }
  
  mgt_info.set_recver(id);
  cmd_van_->Send(mgt_info);
}

void Postmaster::SendReplicaInfo(Container *ctr){
  // TODO send the back up information
  // backup and replica
  // only replicato is sent
  // num = group_.servers()->size();
  LL << " all server size " << group_.servers()->size();
  LL << dhtinfo_.bkp_nodes_.size();
  auto num = dhtinfo_.bkp_nodes_.size();

  for (auto it = dhtinfo_.bkp_nodes_.begin(); it != dhtinfo_.bkp_nodes_.end(); ++it)
  {
    NodeManagementInfo mgt_info;
    mgt_info.set_name(ctr->name());
    mgt_info.set_sender(my_uid_);
    mgt_info.set_command_id(NodeManagementInfo::BACKUP_NODELIST);
    LL << "sending replica info at node " << my_uid();
    uid_t node_uid = it->first.second;
    LL << "preparing for replica for node " << node_uid;
    std::vector< std::vector<uid_t>> backup_list = it->second;
    int num_backup = backup_list.size();
    ReplicaTo *rt = new ReplicaTo();
    // LL << "rt "<< rt->DebugString();
    for (size_t k = 0; k < num_backup; ++k)
    {
      ReplicaList *rl= rt->add_replica_lists();
      CHECK(rl != NULL) << "adding replicalist fault";
      rl->set_replica_id(k);
      std::vector<uid_t> k_backup_list = backup_list[k];
      for (size_t l = 0; l < k_backup_list.size(); ++l)
      {
        KeyRange kr = dhtinfo_.bkp_workloads_[make_tuple(ctr->name(), node_uid, k_backup_list[l])].key_range();
        LL << " back up node info " << node_uid << k_backup_list[l] << kr.ToString();
        if (node_uid == 0) {
          replicato_[make_tuple(ctr->name(), k, k_backup_list[l])] = kr;
        } else {
          MsgKeyRange *msg_kr = rl->add_msg_key_ranges();
          CHECK(msg_kr != NULL) << "adding msgkeurange fault";
          msg_kr->set_node_id(k_backup_list[l]);
          msg_kr->set_key_start(kr.start());
          msg_kr->set_key_end(kr.end());
        }
      }
    }
    
    LL << " back up node info for node " << node_uid << " finished ";
    // LL << rt->DebugString();
    mgt_info.set_allocated_replica_to(rt);
    // LL << rt->DebugString();
    LL << " set replicato finished ";
    mgt_info.set_recver(node_uid);
    if (node_uid != 0) {
      LL << " sending replica info to before" << node_uid;
      CHECK((cmd_van_->Send(mgt_info)).ok() == true ) << "send failed";
      LL << " sending replica info to " << node_uid;
    } else {
      LL << "replica to at node 0 initialized";
    }
    
  }
}

void Postmaster::SlaveAssignNodes(Container *ctr, KeyRange whole) {
  // get the key range from the blocking queue
  // a stupid implementation, keep enquiring the queue until
  // the correct container is found
  LL << "slave assign nodes";
  ReceiveKeyRange(ctr, whole);
  LL << "finishe recived keyranges at " << my_uid();
  if (IamServer())
  {
    LL << "starting to receive replica info at node: " << my_uid();
    ReceiveReplicaInfo(ctr);
    LL << "finished to receive replica info at node: " << my_uid();
  }
}

void Postmaster::ReceiveKeyRange(Container *ctr, KeyRange whole) {
  while (1)
  {
    NodeManagementInfo mgt_info = keyrange_queue_.Take();
    // TODO how to distinguish different containers
    if ((ctr->name().compare(mgt_info.name()) == 0) && (mgt_info.recver() == my_uid_))
    {
      containers_[ctr->name()] = ctr;
      NodeGroup group;
      size_t num = mgt_info.msg_key_ranges_size();
      for (size_t i = 0; i < num; ++i)
      {
        MsgKeyRange mkr = mgt_info.msg_key_ranges(i);
        uid_t id = mkr.node_id();
        KeyRange kr(mkr.key_start(), mkr.key_end());
        LL << "key start: "<< mkr.key_start() << "key end: " << mkr.key_end();
        if (!kr.Valid()) continue;
        Workload wl(id, kr);
        group.servers()->push_back(id);
        group.all()->push_back(id);
        workloads_[make_pair(ctr->name(), id)] = wl;
      }

      num = group_.clients()->size();
      for (size_t i = 0; i < num; ++i) {
        uid_t id = group_.clients()->at(i);
        Workload wl(id, whole, DataRange());
        workloads_[make_pair(ctr->name(), id)] = wl;
        group.clients()->push_back(id);
        group.all()->push_back(id);
      }

      // keep this in the slave ?????
      // divde data evenly to client nodes
      // num = group_.clients()->size();
      // for (size_t i = 0; i < num; ++i) {
      //   uid_t id = group_.clients()->at(i);
      //   Workload wl(id, ctr->key_range(), DataRange());
      //   workloads_[make_pair(ctr->name(), id)] = wl;
      //   group.clients()->push_back(id);
      //   group.all()->push_back(id);
      // }
      // set a root, it is used for doing allreduce
      group.set_root(Node::GetUid("server", 0));
      nodegroups_[ctr->name()] = group;
      // set a virtual all servers workload, TODO also set other groups?
      break;
    }
    else
      keyrange_queue_.Put(mgt_info);
  }
  LL << "finished recive keys at " << ctr->name() << " at node "<< my_uid_;
}

void Postmaster::ReceiveReplicaInfo(Container *ctr) {
  while (1) {
    NodeManagementInfo mgt_info = replicainfo_.Take();
    // TODO how to distinguish different containers
    // strcmp ??
    if (ctr->name().compare(mgt_info.name()) == 0) {
      CHECK(mgt_info.has_replica_to());
      LL << "has replicato";
      // CHECK(mgt_info.has_replicai_from());
      ReplicaTo rt = mgt_info.replica_to();
      size_t num = rt.replica_lists_size();
      for (size_t i = 0; i < num; ++i) {
        ReplicaList rl = rt.replica_lists(i);
        int r_id = rl.replica_id();
        int replica_num = rl.msg_key_ranges_size();
        for (int i = 0; i < replica_num; ++i) {
          MsgKeyRange mkr = rl.msg_key_ranges(i);
          uid_t id = mkr.node_id();
          KeyRange kr(mkr.key_start(), mkr.key_end());
          replicato_[make_tuple(ctr->name(), r_id, id)] = kr;
        }
      }
      break;

      // ReplicaFrom rf = mgt_info.replica_from();
      // num = rf.replica_nodes_size();
      // for (size_t i = 0; i < num; ++i)
      // {
      //   ReplicaNode rn = rf.replica_nodes(i);
      //   CHECK(rn.replica_ids_size() == rn.msg_key_ranges_size());
      //   int replica_num = rn.msg_key_ranges_size();
      //   for (int i = 0; i < replica_num; ++i)
      //   {
      //     int r_id = rn.replica_ids(i);
      //     MsgKeyRange mkr = rn.msg_key_ranges(i);
      //     uid_t id = mkr.node_id();
      //     KeyRange kr(mkr.key_start(), mkr.key_end());
      //     replicafrom_[make_tuple(ctr->name(), id, r_id)] = kr;
      //   }
      // }
    }
    else
      replicainfo_.Put(mgt_info);
  }
}

void Postmaster::send_cmd() {
  // ping all nodes and wait for ack
  // if a node fail, broadcast to all nodes and
  // active dee back node to load the memory
  if (my_uid_ == 0) {
    //sleep(5);
    while (1) {
      size_t num = group_.servers()->size();
      for (size_t i = 0; i < num; ++i) {
        if (group_.servers()->at(i) != 0) {
          if (!PingNode(group_.servers()->at(i)))
            DealWithFailureNode(group_.servers()->at(i));
          else
            LL << "receive ack at node " << group_.servers()->at(i);
        }
      }
      sleep(FLAGS_ping_period);
    }
  }
}

bool Postmaster::PingNode(uid_t id) {
  NodeManagementInfo mgt_info;
  // mail for node management
  // container name does not matter
  mgt_info.set_name("");
  // actually not required info
  mgt_info.set_sender(my_uid_);
  // tell all nodes the key range
  mgt_info.set_command_id(NodeManagementInfo::SERVER_PING);
  mgt_info.set_recver(id);
  cmd_van_->Send(mgt_info);

  NodeManagementInfo reply;

  // wait for the package: set change the value to set the delay
  sleep(FLAGS_wait_seconds);
  Status sts = cmd_van_->Recv(&reply, false);
  //LL << "ping node recv status: " << sts.ToString();
  //if(!sts.ok())
  //  return false;

  CHECK(reply.IsInitialized() == true) << "reply is not initialized";
  CHECK(reply.has_command_id() == true) << "reply has no command id";
  CHECK(reply.command_id() == NodeManagementInfo::SERVER_ACK) << "reply flag is not an ack";
  return true;
}

void Postmaster::BroadcastDead(uid_t id) {
  NodeManagementInfo mgt_info;
  mgt_info.set_name("");
  mgt_info.set_sender(my_uid_);
  // tell all nodes the key range
  mgt_info.set_command_id(NodeManagementInfo::NOTIFY_DEAD);
  mgt_info.set_failed_node_id(id);
  for (size_t j = 0; j < group_.all()->size(); ++j) {
    mgt_info.set_recver(group_.all()->at(j));
    cmd_van_->Send(mgt_info);
  }
}

bool Postmaster::ActivateBackup(FailedNode fn, string name, uid_t recver) {
  NodeManagementInfo mgt_info;
  mgt_info.set_name(name);
  mgt_info.set_sender(my_uid());
  mgt_info.set_command_id(NodeManagementInfo::ACTIVATE_BACKUP);
  mgt_info.set_recver(recver);
  cmd_van_->Send(mgt_info);
  NodeManagementInfo reply;
  cmd_van_->Recv(&reply);
  CHECK(reply.command_id() == NodeManagementInfo::PROCESS_EXECUTED) << "process is not executed";
  CHECK(reply.sender() == recver) << "reply not not from node " << recver;
  return true;
}

void Postmaster::AddBackupNode(int32 id, string name, KeyRange kr) {
  std::vector<string> s_addr = split(FLAGS_server_address, ',');
  CHECK_GE(s_addr.size(), id)
      << "#address in " << FLAGS_server_address << " is less than num_server";
  Node node;
  string mail_addr = s_addr[id - 1];
  std::vector<string> part = split(mail_addr, ':');
  int port = std::stoi(part.back());
  // port + 1 used to send cmd
  port ++;
  string cmd_addr;
  for (size_t j = 0; j < part.size(); j++) {
    if (j != part.size() - 1 )
      cmd_addr += (part[j] + ':');
    else
      cmd_addr += std::to_string(port);
  }
  node.Init(Node::kTypeServer, id, mail_addr, cmd_addr);
  uid_t uid = node.uid();
  all_[uid] = node;
  group_.servers()->push_back(uid);
  group_.all()->push_back(uid);
  NodeGroup group = nodegroups_[name];
  CHECK(kr.Valid()) << "new key is invalid";
  Workload wl(id, kr);
  group.servers()->push_back(id);
  group.all()->push_back(id);
  workloads_[make_pair(name, id)] = wl;
}

void Postmaster::BroadcastAddNode(Node nd, KeyRange kr) {
  NodeManagementInfo mgt_info;
  mgt_info.set_command_id(NodeManagementInfo::ADD_NEW_NODE);
  mgt_info.set_sender(my_uid());
  NewNode *nw = new NewNode();
  nw->set_server_address(nd.addr());
  nw->set_cmd_address(nd.cmd_addr());
  MsgKeyRange msg_range;
  msg_range.set_key_start(kr.start());
  msg_range.set_key_end(kr.end());
  msg_range.set_node_id(nd.uid());
  for (size_t j = 0; j < group_.all()->size(); ++j) {
    auto recver = group_.all()->at(j);
    if (recver != nd.uid()) {
      mgt_info.set_recver(group_.all()->at(j));
      cmd_van_->Send(mgt_info);
    }
  }
}

void Postmaster::DealWithFailureNode(uid_t failed_id) {
  // a simplified version: notify all clients(servers) here
  // TODO notify based on container, only those containers with the dead node
  // send the notify and new node info separately
  // remove the node locally first
  RemoveNode(failed_id);
  BroadcastDead(failed_id);
  // rescue the nodes
  // call the dht to get the replica nodes to notify
  // distinguish the containers!!!!!!!
  map<uid_t, KeyRange> replica_node_info;
  // iterate each container and activate the backup of each container
  int temp_id = num_server_;
  for (auto it = containers_.begin(); it != containers_.end(); ++it) {
    string name = it->first;
    replica_node_info = GetReplicaTo(name, 0);
    for (auto pair = replica_node_info.begin(); pair != replica_node_info.end(); ++pair) {
      uid_t id = pair->first;
      KeyRange kr = pair->second;
      FailedNode fn;
      fn.set_failed_node_id(failed_id);
      fn.set_new_node_id(temp_id);
      uid_t new_id = Node::GetUid("s", temp_id);
      temp_id++;
      fn.set_replica_to_use(0);
      CHECK(ActivateBackup(fn, name, id)) << " activate node " << id << " for failed node " << failed_id << " failed ";
      NodeManagementInfo reply_ack;
      cmd_van_->Recv(&reply_ack);
      CHECK(reply_ack.command_id() == NodeManagementInfo::BACKUP_LOADED) << " backup loaded failure";
      CHECK(reply_ack.sender() == new_id) << "new back up node id is wrong";
      
      // add the new node to the group
      AddBackupNode(temp_id, name, kr);
      BroadcastAddNode(all_[Node::GetUid("s", temp_id)], kr);
      SendKeyRange(containers_[name], new_id);
    }
  }
  // for the nodes in the list set the replia id then send the backup
  // send a new node id to the replica manger so that ...
  // wait for the ack and then
  // send key range info???

  // send add node command to all clients (servers)??
}

void Postmaster::receive_cmd() {
  // fetch the mail and
  // process the command
  // notify a node is dead
  // update the node info
  // update the responsible key ranges
  // update the backup info
  //
  if (my_uid_ != 0) {
    NodeManagementInfo mgt_info;
    while (1) {
      cmd_van_->Recv(&mgt_info);
      CHECK(mgt_info.sender() == 0);
      // LL << "receive mgtinfo " << mgt_info.command_id() << " at node " << my_uid();
      switch (mgt_info.command_id())
      {
        case NodeManagementInfo::SERVER_MSG_KEY_RANGE:
          break;
        case NodeManagementInfo::CLIENT_MSG_KEY_RANGE:
          // distinguish different containers
          keyrange_queue_.Put(mgt_info);
          break;
        case NodeManagementInfo::BACKUP_NODELIST:
          // think about how to store the informaiton
          LL << " received replica info at node " << my_uid();
          replicainfo_.Put(mgt_info);
          break;
        case NodeManagementInfo::NOTIFY_DEAD:
          {
            // remove the node keyrange info from the list
            // no need to specify the container
            uid_t id = mgt_info.failed_node_id();
            RemoveNode(id);
            break;
          }
        case NodeManagementInfo::ADD_NEW_NODE:
          {
            // add a new node and a key range
            NewNode nw = mgt_info.new_node();
            MsgKeyRange mkr = nw.range();
            KeyRange kr(mkr.key_start(), mkr.key_end());
            int id = mkr.node_id();
            Node node(Node::kTypeServer, id, nw.server_address(), nw.cmd_address());
            AddNode(mgt_info.name(), node, kr);
            break;
          }
        case NodeManagementInfo::SERVER_PING:
          LL << "receive server ping at node:" << my_uid_;
          // set the ack and send back
          Ack();
          break;
        case NodeManagementInfo::ACTIVATE_BACKUP:
          ExecuteCmd(mgt_info);
          break;
        default:
          break;
      }
    }
  }
}

void Postmaster::RemoveNode(uid_t id) {
  auto index = find(group_.all()->begin(), group_.all()->end(), id);
  CHECK(index != group_.all()->end()) << "can not find failure node in group_.all";
  group_.all()->erase(index);
  index = find(group_.servers()->begin(), group_.servers()->end(), id);
  CHECK(index != group_.servers()->end()) << "can not find failure node in group_.servers_";
  group_.servers()->erase(index);
  num_server_--;
  for (auto ctrpair : containers_)
  {
    Container *ctr = ctrpair.second;
    string name = ctr->name();
    NodeGroup group = nodegroups_[name];
    auto index = find(group_.all()->begin(), group_.all()->end(), id);
    group.all()->erase(index);
    index = find(group.servers()->begin(), group.servers()->end(), id);
    group.servers()->erase(index);
    workloads_.erase(make_pair(name, id));
  }
}

void Postmaster::AddNode(string name, Node node, KeyRange kr) {
  // Node node(Node::kTypeServer, id, server_address, cmd_address);
  uid_t uid = node.uid();
  all_[uid] = node;
  group_.servers()->push_back(uid);
  group_.all()->push_back(uid);
  // connect to the new node
  CHECK(van_->Connect(node, 0));
  CHECK(cmd_van_->Connect(node, 1));
  // CHECK the container exists
  CHECK(containers_.find(name) != containers_.end()) << "cannot find continer in add node";
  NodeGroup nodegroup = nodegroups_[name];
  nodegroup.servers()->push_back(uid);
  nodegroup.all()->push_back(uid);
  Workload wl(uid, kr);
  workloads_[make_pair(name, uid)] = wl;
  num_server_++;

}
void Postmaster::RescueAck(string name) {
  CHECK(IamServer()) << " I am not server";
  NodeManagementInfo mgt_info;
  mgt_info.set_name("name");
  mgt_info.set_sender(my_uid_);
  mgt_info.set_recver(0);
  // tell all nodes the key range
  mgt_info.set_command_id(NodeManagementInfo::BACKUP_LOADED);
  cmd_van_->Send(mgt_info);
}

void Postmaster::Ack() {
  CHECK(IamServer()) << "I am not a server";
  NodeManagementInfo mgt_info;
  mgt_info.set_name("");
  mgt_info.set_sender(my_uid_);
  mgt_info.set_recver(0);
  // tell all nodes the key range
  mgt_info.set_command_id(NodeManagementInfo::SERVER_ACK);
  cmd_van_->Send(mgt_info);
}

map<uid_t, KeyRange> Postmaster::GetReplicaTo(string name, int32 ith_replica) {
  map<uid_t, KeyRange> replica_node_info;
  for (auto it = replicato_.begin(); it != replicato_.end(); ++it) {
    tuple<string, int32, uid_t> index = it->first;
    KeyRange kr = it->second;
    string ctr_name = std::get<0>(index);
    int32 idx_replica = std::get<1>(index);
    uid_t id = std::get<2>(index);
    if ((ctr_name == name) && (idx_replica == ith_replica))
      replica_node_info[id] = kr;
  }
  return replica_node_info;
}

void Postmaster::ExecuteCmd(NodeManagementInfo mgt_info) {
  CHECK(mgt_info.has_failed_node());
  FailedNode fn = mgt_info.failed_node();
  uid_t failed_id = fn.failed_node_id();
  CHECK(failed_id / 2 == 0) << "failed node is is not a server";
  uid_t new_id = fn.new_node_id();
  int32 replica = fn.replica_to_use();
  int32 num_server = fn.num_server();
  string cmd1 = StrCat("../fault_tolerance_press --my_type=server --my_rank=", new_id);
  string cmd2 = StrCat("--num_server=", num_server, "--num_client=", FLAGS_num_client);
  string cmd3 = StrCat("--server_address=", FLAGS_server_address, "--client_address=", FLAGS_client_address);
  string cmd4 = StrCat("--global_feature_num=", FLAGS_global_feature_num, "--local_feature_num=", FLAGS_local_feature_num);
  string cmd5 = StrCat("--is_backup_process");
  string cmd6 = StrCat("--enable_fault_tolerance &> ft.log.server.${id} &");
  string cmd = StrCat(cmd1, cmd2, cmd3, cmd4, cmd5, cmd6);
  LL << "Start activating backups...";
  system(cmd.c_str());
  NodeManagementInfo reply;
  reply.set_command_id(NodeManagementInfo::PROCESS_EXECUTED);
  reply.set_name(mgt_info.name());
  reply.set_recver(0);
  reply.set_sender(my_uid());
  cmd_van_->Send(reply);
}

const NodeGroup& Postmaster::GetNodeGroup(const string& name) const {
  const auto& it = nodegroups_.find(name);
  CHECK(it != nodegroups_.end());
  return it->second;
}

Workload* Postmaster::GetWorkload(const string& name, uid_t id) {
  const auto& it = workloads_.find(make_pair(name, id));
  CHECK(it != workloads_.end());
  return &it->second;
}

Container* Postmaster::GetContainer(const string& name) const {
  const auto& it = containers_.find(name);
  CHECK(it != containers_.end()) << "unknow container: " << name;
  return it->second;
}

// void Postmaster::AssignNodes(Container *ctr) {
//
// }

// void Postmaster::DealWithFailureNode() {
//   locate the backup data in the cluster
//   tell them to load backup data
//   receive their ack
//   update every clients' node key range info
// }
// void Postmaster::PostmasterThread() {
// bool is_master;
// Get Command
// Switch(Command) {
//   if (master && a node fail)
//    DealWithFailureNode();
//   if (ask to update node key range)
//    update node key range info
// }
// }

}
