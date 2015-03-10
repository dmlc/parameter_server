#include "system/manager.h"
#include "system/postoffice.h"
#include "system/customer.h"
namespace PS {

DEFINE_int32(num_servers, 0, "number of servers");
DEFINE_int32(num_workers, 0, "number of clients");
DEFINE_int32(num_threads, 2, "number of computational threads");
DEFINE_int32(num_replicas, 0, "number of replicas per server node");

DEFINE_string(app_conf, "", "the string configuration of app");
DEFINE_string(app_file, "", "the configuration file of app");

DECLARE_int32(report_interval);

DEFINE_bool(verbose, false, "");

Manager::Manager() {}

Manager::~Manager() {
  for (auto& it : customers_) {
    if (it.second.second) delete it.second.first;
  }
  delete node_assigner_;
  delete app_;
}

void Manager::init(char* argv0) {
  van_.init(argv0);

  if (isScheduler()) {
    NOTICE("Staring system... info are logged in %s/%s.log.*",
           FLAGS_log_dir.c_str(), basename(argv0));

    node_assigner_ = new NodeAssigner(FLAGS_num_servers);
    // create the app
    if (!FLAGS_app_file.empty()) {
      CHECK(readFileToString(FLAGS_app_file, &app_conf_))
          << " failed to read conf file " << FLAGS_app_file;
    }
    app_conf_ += FLAGS_app_conf;
    createApp(app_conf_);

    // add my node into app_
    addNode(van_.myNode());
  } else {
    // request the app config from the scheduler
    Task task = newControlTask(Control::REQUEST_APP);
    *task.mutable_ctrl()->add_node() = van_.myNode();
    sendTask(van_.scheduler(), task);
  }
}

void Manager::run() {
  // app_ is created by postoffice::recv_thread (except for the scheduler),
  // while run() is called by the main thread, so here should be a thread
  // synchronization.
  app_promise_.get_future().wait();
  app_->run();
}

void Manager::stop() {
  if (isScheduler()) {
    // wait all other nodes are ready for exit
    while (num_active_nodes_ > 1) usleep(500);
    // broadcast the terminate signal
    for (const auto& it : nodes_) {
      Task task = newControlTask(Control::EXIT);
      sendTask(it.second, task);
    }
    usleep(800);
    LOG(INFO) << "System stopped";
  } else {
    Task task = newControlTask(Control::READY_TO_EXIT);
    sendTask(van_.scheduler(), task);

    // run as a daemon until received the termination message
    while (!done_) usleep(500);
  }
}


bool Manager::process(const MessagePtr& msg) {
  const Task& task = msg->task;
  CHECK(task.control());

  if (task.request()) {
    CHECK(task.has_ctrl());
    const auto& ctrl = task.ctrl();
    switch (ctrl.cmd()) {
      case Control::REQUEST_APP: {
        CHECK(isScheduler());
        // need to connect to this node before sending reply message
        CHECK_EQ(ctrl.node_size(), 1);
        CHECK(van_.connect(ctrl.node(0)));
        Task reply = task;
        reply.set_request(false);
        reply.set_msg(app_conf_);
        sendTask(msg->sender, reply);
        msg->replied = true;
        break;
      }
      case Control::REGISTER_NODE: {
        CHECK(isScheduler());
        CHECK_EQ(ctrl.node_size(), 1);
        Node sender = ctrl.node(0);
        CHECK_NOTNULL(node_assigner_)->assign(&sender);
        addNode(sender);
        break;
      }
      case Control::REPORT_PERF: {
        CHECK(isScheduler());
        // TODO
        break;
      }
      case Control::READY_TO_EXIT: {
        CHECK(isScheduler());
        -- num_active_nodes_;
        break;
      }
      case Control::ADD_NODE:
      case Control::UPDATE_NODE: {
        for (int i = 0; i < ctrl.node_size(); ++i) {
          addNode(ctrl.node(i));
        }
        break;
      }
      case Control::REPLACE_NODE: {
        // TODO
        break;
      }
      case Control::REMOVE_NODE: {
        // TODO
        break;
      }
      case Control::EXIT: {
        done_ = true;
        return false;
      }
    }
    if (!msg->replied) Postoffice::instance().reply(msg->sender, msg->task);
  } else {
    if (!task.has_ctrl()) return true;
    if (task.ctrl().cmd() == Control::REQUEST_APP) {
      CHECK(task.has_msg());
      createApp(task.msg());
      // app is created, now we can ask the scheduler to broadcast this node to others
      Task task = newControlTask(Control::REGISTER_NODE);
      *task.mutable_ctrl()->add_node() = van_.myNode();
      sendTask(van_.scheduler(), task);
    }
  }
  return true;
}

void Manager::addNode(const Node& node) {
  // add to system
  if (nodes_.find(node.id()) == nodes_.end()) {
    if (!isScheduler()) {
      // the scheduler has already connect this node when processing REQUEST_APP
      CHECK(van_.connect(node));
    }
    if (node.role() == Node::WORKER) ++ num_workers_;
    if (node.role() == Node::SERVER) ++ num_servers_;
    ++ num_active_nodes_;
  }
  nodes_[node.id()] = node;

  // add to app
  for (auto& it : customers_) {
    it.second.first->exec().addNode(node);
  }

  if (isScheduler() && node.id() != van_.myNode().id()) {
    // send all existing nodes info to sender
    Task add_node = newControlTask(Control::ADD_NODE);
    for (const auto& it : nodes_) {
      *add_node.mutable_ctrl()->add_node() = it.second;
    }
    sendTask(node, add_node);

    // broadcast this new sender info
    for (const auto& it : nodes_) {
      if (it.first == van_.myNode().id() || it.first == node.id()) {
        continue;
      }
      Task add_new_node = newControlTask(Control::ADD_NODE);
      *add_new_node.mutable_ctrl()->add_node() = node;
      sendTask(it.second, add_new_node);
    }
  }

  LOG(INFO) << "add node: " << node.ShortDebugString();
}


void Manager::removeNode(const NodeID& node_id) {
  // TODO use *replace* for server
  auto it = nodes_.find(node_id);
  if (it == nodes_.end()) return;
  Node node = it->second;

  // remove from customers
  for (auto& it : customers_) {
    it.second.first->exec().removeNode(node);
  }

  // van_.disconnect(node);
  if (node.role() == Node::WORKER) -- num_workers_;
  if (node.role() == Node::SERVER) -- num_servers_;
  -- num_active_nodes_;
  nodes_.erase(it);

  LOG(INFO) << "remove node: " << node.ShortDebugString();
}

void Manager::nodeDisconnected(const NodeID node_id) {
  if (isScheduler()) {
    // broadcast the dead node info
    LOG(INFO) << node_id << " is disconnected";
  } else {
    // sleep a while, in case this node is already in terminating
    for (int i = 0; i < 1000; ++i) {
      usleep(1000);
      if (done_) return;
    }
    LOG(ERROR) << van_.myNode().id() << ": the scheduler is died, killing myself";
    string kill = "kill -9 " + std::to_string(getpid());
    system(kill.c_str());
  }
}

Task Manager::newControlTask(Control::Command cmd) {
  Task task;
  task.set_control(true);
  task.set_request(true);
  task.set_time(isScheduler() ? time_ * 2 : time_ * 2 + 1);
  ++ time_;
  task.mutable_ctrl()->set_cmd(cmd);
  return task;
}

void Manager::sendTask(const NodeID& recver, const Task& task) {
  MessagePtr msg(new Message(task));
  msg->recver = recver;
  Postoffice::instance().queue(msg);
}

void Manager::createApp(const string& conf) {
  app_ = App::create(conf);
  CHECK(app_ != NULL)
      << ": failed to create app with conf\n" << app_conf_;
  app_promise_.set_value();
}

void Manager::waitServersReady() {
  while (num_servers_ < FLAGS_num_servers) {
    usleep(500);
  }
}
void Manager::waitWorkersReady() {
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
void Manager::addCustomer(Customer* obj) {
  CHECK_EQ(customers_.count(obj->id()), 0)
      << obj->id() << " already exists";
  customers_[obj->id()] = std::make_pair(obj, false);
}

void Manager::removeCustomer(int id) {
  auto it = customers_.find(id);
  // only assign it to NULL, because the call chain could be:
  // ~CustomerManager() -> ~Customer() -> remove(int id)
  if (it != customers_.end()) it->second.first = NULL;
}

int Manager::nextCustomerID() {
  int id = 0;
  for (const auto& it : customers_) id = std::max(id, it.second.first->id()+1);
  return id;
}

} // namespace PS
