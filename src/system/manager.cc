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
  delete node_assigner_;
  delete app_;
  for (auto& it : customers_) {
    if (it.second.second) delete it.second.first;
  }
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
  const auto& ctrl = task.ctrl();

  if (task.request()) {
    switch (ctrl.cmd()) {
      case Control::REQUEST_APP: {
        CHECK(isScheduler());
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
    if (ctrl.cmd() == Control::REQUEST_APP) {
      createApp(task.msg());
      // app is created, now we can ask the scheduler to broadcast this node to others
      Task task = newControlTask(Control::REGISTER_NODE);
      sendTask(van_.scheduler(), task);
    }
  }
  return true;
}

void Manager::addNode(const Node& node) {
  // add to system
  if (nodes_.find(node.id()) == nodes_.end()) {
    CHECK(van_.connect(node));
    if (node.role() == Node::WORKER) ++ num_workers_;
    if (node.role() == Node::SERVER) ++ num_servers_;
    ++ num_active_nodes_;
  }
  nodes_[node.id()] = node;

  // add to app
  for (auto& it : customers_) {
    it.second.first->exec().addNode(node);
  }

  if (isScheduler()) {
    // // send all existing nodes info to sender
    // Task add_node = newControlTask(Control::ADD);
    // for (const auto& it : nodes_) {
    //   *add_node.mutable_control()->add_node() = it.second;
    // }
    // sendTask(sender, add_node);

    // // broadcast this new sender info
    // for (const auto& it : nodes_) {
    //   if (it.first == van_.myNode().id() || it.first == sender.id()) {
    //     continue;
    //   }
    //   Task add_new_node = newControlTask(Control::ADD);
    //   *add_new_node.mutable_control()->add_node() = sender;
    //   sendTask(it.second, add_new_node);
    // }
  }

  LOG(INFO) << "add node: " << node.ShortDebugString();
}


void Manager::removeNode(const Node& node) {
  // TODO use *replace* for server
  if (node.role() == Node::WORKER) {
    for (auto& it : customers_) {
      it.second.first->exec().removeNode(node);
    }
  }

  van_.disconnect(node);
  if (nodes_.find(node.id()) != nodes_.end()) {
    if (node.role() == Node::WORKER) -- num_workers_;
    if (node.role() == Node::SERVER) -- num_servers_;
    nodes_.erase(node.id());
    -- num_active_nodes_;
  }

  LOG(INFO) << "remove node: " << node.ShortDebugString();
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

// customers
// void Manager::addCustomer(Customer* customer) {
//   CHECK_EQ(customers_.count(customer->name()), 0) << customer->name();
//   customers_[customer->name()] = std::make_pair(customer, false);
// }

// Customer* Manager::customer(const string& name) {
//   auto it = customers_.find(name);
//   if (it == customers_.end()) return nullptr;
//   return it->second.first;
// }

// void Manager::removeCustomer(const string& name) {
//   auto it = customers_.find(name);
//   if (it == customers_.end()) return;
//   customers_.erase(it);
// }

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

} // namespace PS
