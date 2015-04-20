#include "system/manager.h"
#include "system/postoffice.h"
#include "system/customer.h"
namespace PS {

DECLARE_int32(num_servers);
DECLARE_int32(num_workers);
DECLARE_int32(num_replicas);
DECLARE_int32(report_interval);

DEFINE_string(app_conf, "", "the string configuration of app");
DEFINE_string(app_file, "", "the configuration file of app");

DEFINE_uint64(key_start, 0, "global key range");
DEFINE_uint64(key_end, kuint64max, "global key range");

Manager::Manager() {}
Manager::~Manager() {
  for (auto& it : customers_) {
    if (it.second.second) delete it.second.first;
  }
  delete node_assigner_;
  delete app_;
}

void Manager::Init(char* argv0) {
  env_.Init(argv0);
  van_.Init();

  if (IsScheduler()) {
    if (!FLAGS_logtostderr) {
      NOTICE("Staring system. Logging into %s/%s.log.*",
             FLAGS_log_dir.c_str(), basename(argv0));
    }

    node_assigner_ = new NodeAssigner(
        FLAGS_num_servers, Range<Key>(FLAGS_key_start, FLAGS_key_end));
    // create the app
    if (!FLAGS_app_file.empty()) {
      CHECK(readFileToString(FLAGS_app_file, &app_conf_))
          << " failed to read conf file " << FLAGS_app_file;
    }
    app_conf_ += FLAGS_app_conf;
    CreateApp(app_conf_);

    // add my node into app_
    AddNode(van_.my_node());
  } else {
    // request the app config from the scheduler
    Task task = NewControlTask(Control::REQUEST_APP);
    *task.mutable_ctrl()->add_node() = van_.my_node();
    SendTask(van_.scheduler(), task);
  }
}

void Manager::Run() {
  // app_ is created by postoffice::recv_thread (except for the scheduler),
  // while run() is called by the main thread, so here should be a thread
  // synchronization.

  // wait my node info is updated
  while (!is_my_node_inited_) usleep(500);
  if (van_.my_node().role() == Node::WORKER) {
    WaitServersReady();
  }
  VLOG(1) << "run app..";
  CHECK_NOTNULL(app_)->Run();
}

void Manager::Stop() {
  if (IsScheduler()) {
    // wait all other nodes are ready for exit
    while (num_active_nodes_ > 1) usleep(500);
    // broadcast the terminate signal
    in_exit_ = true;
    for (const auto& it : nodes_) {
      Task task = NewControlTask(Control::EXIT);
      SendTask(it.second, task);
    }
    usleep(800);
    LOG(INFO) << "System stopped";
  } else {
    Task task = NewControlTask(Control::READY_TO_EXIT);
    SendTask(van_.scheduler(), task);

    // run as a daemon until received the termination message
    while (!done_) usleep(500);
  }
}


bool Manager::Process(Message* msg) {
  const Task& task = msg->task;
  CHECK(task.control());

  if (task.request()) {
    Task reply;
    reply.set_control(true);
    reply.set_request(false);
    reply.set_time(task.time());

    CHECK(task.has_ctrl());
    const auto& ctrl = task.ctrl();
    switch (ctrl.cmd()) {
      case Control::REQUEST_APP: {
        CHECK(IsScheduler());
        // need to connect to this node before sending reply message
        CHECK_EQ(ctrl.node_size(), 1);
        CHECK(van_.Connect(ctrl.node(0)));
        reply.mutable_ctrl()->set_cmd(Control::REQUEST_APP);
        reply.set_msg(app_conf_);
        break;
      }
      case Control::REGISTER_NODE: {
        CHECK(IsScheduler());
        CHECK_EQ(ctrl.node_size(), 1);
        Node sender = ctrl.node(0);
        CHECK_NOTNULL(node_assigner_)->assign(&sender);
        AddNode(sender);
        break;
      }
      case Control::REPORT_PERF: {
        CHECK(IsScheduler());
        // TODO
        break;
      }
      case Control::READY_TO_EXIT: {
        CHECK(IsScheduler());
        -- num_active_nodes_;
        break;
      }
      case Control::ADD_NODE:
      case Control::UPDATE_NODE: {
        for (int i = 0; i < ctrl.node_size(); ++i) {
          AddNode(ctrl.node(i));
        } break;
      }
      case Control::REPLACE_NODE: {
        // TODO
        break;
      }
      case Control::REMOVE_NODE: {
        for (int i = 0; i < ctrl.node_size(); ++i) {
          RemoveNode(ctrl.node(i).id());
        } break;
      }
      case Control::EXIT: {
        done_ = true;
        return false;
      }
    }
    SendTask(msg->sender, reply);
  } else {
    if (!task.has_ctrl()) return true;
    if (task.ctrl().cmd() == Control::REQUEST_APP) {
      CHECK(task.has_msg());
      CreateApp(task.msg());
      // app is created, now we can ask the scheduler to broadcast this node to others
      Task task = NewControlTask(Control::REGISTER_NODE);
      *task.mutable_ctrl()->add_node() = van_.my_node();
      SendTask(van_.scheduler(), task);
    }
  }
  return true;
}

void Manager::AddNode(const Node& node) {
  // add to system
  nodes_mu_.lock();
  if (nodes_.find(node.id()) == nodes_.end()) {
    if (!IsScheduler()) {
      // the scheduler has already connect this node when processing REQUEST_APP
      CHECK(van_.Connect(node));
    }
    if (node.role() == Node::WORKER) ++ num_workers_;
    if (node.role() == Node::SERVER) ++ num_servers_;
    ++ num_active_nodes_;
  }
  nodes_[node.id()] = node;
  nodes_mu_.unlock();

  // add to app
  for (auto& it : customers_) {
    it.second.first->executor()->AddNode(node);
  }

  if (IsScheduler() && node.id() != van_.my_node().id()) {
    // send all existing nodes info to sender
    Task add_node = NewControlTask(Control::ADD_NODE);
    for (const auto& it : nodes_) {
      *add_node.mutable_ctrl()->add_node() = it.second;
    }
    SendTask(node, add_node);

    // broadcast this new sender info
    for (const auto& it : nodes_) {
      if (it.first == van_.my_node().id() || it.first == node.id()) {
        continue;
      }
      Task add_new_node = NewControlTask(Control::ADD_NODE);
      *add_new_node.mutable_ctrl()->add_node() = node;
      SendTask(it.second, add_new_node);
    }
  }

  if (node.id() == van_.my_node().id()) is_my_node_inited_ = true;
  VLOG(1) << "add node: " << node.ShortDebugString();
}


void Manager::RemoveNode(const NodeID& node_id) {
  nodes_mu_.lock();
  auto it = nodes_.find(node_id);
  if (it == nodes_.end()) return;
  Node node = it->second;
  // van_.disconnect(node);
  if (node.role() == Node::WORKER) -- num_workers_;
  if (node.role() == Node::SERVER) -- num_servers_;
  -- num_active_nodes_;
  nodes_.erase(it);
  nodes_mu_.unlock();

  // TODO use *replace* for server
  // TODO remove from customers
  // remove from customers
  // for (auto& it : customers_) {
  //   it.second.first->exec().removeNode(node);
  // }

  // remove from app
  for (auto& it : customers_) {
    it.second.first->executor()->RemoveNode(node);
  }

  // broadcast
  if (IsScheduler() && node.id() != van_.my_node().id()) {
    for (const auto& it : nodes_) {
      if (it.first == van_.my_node().id() || it.first == node.id()) {
        continue;
      }
      Task remove_node = NewControlTask(Control::REMOVE_NODE);
      *remove_node.mutable_ctrl()->add_node() = node;
      SendTask(it.second, remove_node);
    }
  }

  VLOG(1) << "remove node: " << node.ShortDebugString();
}

void Manager::NodeDisconnected(const NodeID node_id) {
  // alreay in shutting down?
  if (in_exit_) return;

  // call handlers
  for (const auto& h : node_failure_handlers_) h(node_id);

  if (IsScheduler()) {
    LOG(INFO) << node_id << " is disconnected";
    RemoveNode(node_id);
  } else {
    // sleep a while, in case this node is already in terminating
    for (int i = 0; i < 1000; ++i) {
      usleep(1000);
      if (done_) return;
    }
    LOG(ERROR) << van_.my_node().id() << ": the scheduler is died, killing myself";
    string kill = "kill -9 " + std::to_string(getpid());
    system(kill.c_str());
  }
}

Task Manager::NewControlTask(Control::Command cmd) {
  Task task;
  task.set_control(true);
  task.set_request(true);
  task.set_time(IsScheduler() ? time_ * 2 : time_ * 2 + 1);
  ++ time_;
  task.mutable_ctrl()->set_cmd(cmd);
  return task;
}

void Manager::SendTask(const NodeID& recver, const Task& task) {
  Message* msg = new Message(task);
  msg->recver = recver;
  Postoffice::instance().Queue(msg);
}

void Manager::CreateApp(const string& conf) {
  app_ = App::Create(conf);
  CHECK(app_ != NULL)
      << ": failed to create app with conf\n" << app_conf_;
}

void Manager::WaitServersReady() {
  while (num_servers_ < FLAGS_num_servers) {
    usleep(500);
  }
}
void Manager::WaitWorkersReady() {
  while (num_workers_ < FLAGS_num_workers) {
    usleep(500);
  }
}

// customers
Customer* Manager::customer(int id) {
  auto it = customers_.find(id);
  if (it == customers_.end()) CHECK(false) << id << " does not exist";
  return it->second.first;
}
void Manager::AddCustomer(Customer* obj) {
  CHECK_EQ(customers_.count(obj->id()), 0)
      << obj->id() << " already exists";
  customers_[obj->id()] = std::make_pair(obj, false);
  nodes_mu_.lock();
  for (const auto& it : nodes_) {
    obj->executor()->AddNode(it.second);
  }
  nodes_mu_.unlock();
}

void Manager::RemoveCustomer(int id) {
  auto it = customers_.find(id);
  // only assign it to NULL, because the call chain could be:
  // ~CustomerManager() -> ~Customer() -> remove(int id)
  if (it != customers_.end()) it->second.first = NULL;
}

int Manager::NextCustomerID() {
  int id = 0;
  for (const auto& it : customers_) id = std::max(id, it.second.first->id()+1);
  return id;
}

} // namespace PS
