#include "system/manager.h"
#include "system/postoffice.h"
#include "system/app.h"
namespace PS {

DEFINE_int32(num_servers, 0, "number of servers");
DEFINE_int32(num_workers, 0, "number of clients");
DEFINE_int32(num_threads, 2, "number of computational threads");
DEFINE_int32(num_replicas, 0, "number of replicas per server node");

DEFINE_string(app_name, "app", "the name of this app");
DEFINE_string(app_conf, "", "the string configuration of app");
DEFINE_string(app_file, "", "the configuration file of app");

DECLARE_int32(report_interval);

DEFINE_bool(verbose, false, "");

Manager::Manager() {
  node_assigner_ = new NodeAssigner(FLAGS_num_workers);
}

Manager::~Manager() {
  delete node_assigner_;
  delete app_;
  for (auto& it : customers_) {
    if (it.second.second) delete it.second.first;
  }
}

void Manager::init() {
  van_.init();
  if (isScheduler()) {
    // create the app
    if (!FLAGS_app_file.empty()) {
      CHECK(readFileToString(FLAGS_app_file, &app_conf_))
          << " failed to read conf file " << FLAGS_app_file;
    }
    app_conf_ += FLAGS_app_conf;
    createApp(FLAGS_app_name, app_conf_);

    // add my node into app_
    addNode(van_.myNode());
  } else {
    // connect to the scheduler, which will send back a create_app request
    Task task = newControlTask(Control::CONNECT);
    *task.mutable_control()->add_node() = van_.myNode();
    sendTask(van_.scheduler(), task);
  }
}

void Manager::run() {
  app_promise_.get_future().wait();
  app_->run();
}

void Manager::stop() {
  if (isScheduler()) {
    // wait all machines are done
    while (num_active_nodes_ > 0) usleep(500);
    // broadcast the terminate signal
    for (const auto& it : nodes_) {
      Task task = newControlTask(Control::TERMINATE);
      sendTask(it.second, task);
    }
    usleep(800);
    LOG(INFO) << "System stopped";
  } else {
    Task task = newControlTask(Control::STOP);
    sendTask(van_.scheduler(), task);

    // run as a daemon until received the termination messag
    while (!done_) usleep(500);
  }
}


bool Manager::process(const MessagePtr& msg) {
  const Task& tk = msg->task;
  CHECK_EQ(tk.type(), Task::CONTROL);
  CHECK(tk.has_control());
  const auto& ctrl = tk.control();

  if (tk.request()) {
    switch (ctrl.cmd()) {
      case Control::CONNECT: {  // a node => scheduler
        // create the app in sender
        CHECK(isScheduler());
        CHECK_EQ(ctrl.node_size(), 1);
        Node sender = ctrl.node(0);
        Task app = newControlTask(Control::CREATE_APP, app_->name());
        app.mutable_control()->set_app_conf(app_conf_);
        sendTask(sender, app);

        // only connect this sender, but do not add it to system at this moment
        van_.connect(sender);
        // save the sender info
        new_nodes_[sender.id()] = sender;
        break;
      }
      case Control::CREATE_APP: {  // scheduler => a node
        createApp(tk.customer(), ctrl.app_conf());
        break;
      }
      case Control::HEARTBEAT: {  // a node => scheduler
        // TODO
        break;
      }
      case Control::ADD:
      case Control::UPDATE: {  // scheduler => a node
        for (int i = 0; i < ctrl.node_size(); ++i) {
          addNode(ctrl.node(i));
        }
        break;
      }
      case Control::REPLACE: {
        break;
      }
      case Control::REMOVE: {
        break;
      }
      case Control::STOP: {  // a node => scheduler
        -- num_active_nodes_;
      }
      case Control::TERMINATE: {  // scheduler => a node
        done_ = true;
        return false;
      }
    }
    // reply a message
    Postoffice::instance().reply(msg->sender, msg->task);
  } else {
    if (ctrl.cmd() == Control::CREATE_APP) { // node => scheduler
      // add the sender into system
      CHECK(isScheduler());
      auto it = new_nodes_.find(msg->sender);
      CHECK(it != new_nodes_.end());
      Node sender = it->second;
      CHECK_NOTNULL(node_assigner_)->assign(&sender);
      addNode(sender);
      new_nodes_.erase(it);

      // send all existing nodes info to sender
      Task add_node = newControlTask(Control::ADD);
      for (const auto& it : nodes_) {
        *add_node.mutable_control()->add_node() = it.second;
      }
      sendTask(sender, add_node);

      // broadcast this new sender info
      for (const auto& it : nodes_) {
        if (it.first == van_.myNode().id() || it.first == sender.id()) {
          continue;
        }
        Task add_new_node = newControlTask(Control::ADD);
        *add_node.mutable_control()->add_node() = sender;
        sendTask(it.second, add_new_node);
      }
    }
  }
  return true;
}

void Manager::addNode(const Node& node) {
  // add to system
  if (nodes_.find(node.id()) == nodes_.end()) {
    CHECK(van_.connect(node).ok());
    if (node.role() == Node::WORKER) ++ num_workers_;
    if (node.role() == Node::SERVER) ++ num_servers_;
    ++ num_active_nodes_;
  }
  nodes_[node.id()] = node;

  // add to app
  for (auto& it : customers_) {
    it.second.first->exec().addNode(node);
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

Task Manager::newControlTask(Control::Command cmd, const string& customer_id) {
  Task task;
  task.set_type(Task::CONTROL);
  task.set_request(true);
  if (customer_id.size()) task.set_customer(customer_id);
  task.mutable_control()->set_cmd(cmd);
  return task;
}

void Manager::sendTask(const Node& recver, const Task& task) {
  MessagePtr msg(new Message(task));
  msg->recver = recver.id();
  Postoffice::instance().queue(msg);
}

void Manager::createApp(const string& name, const string& conf) {
  app_ = App::create(name, conf);
  CHECK(app_ != NULL)
      << ": failed to create [" << name << "] with conf\n" << app_conf_;
  app_promise_.set_value();
}

void Manager::addCustomer(Customer* customer) {
  CHECK_EQ(customers_.count(customer->name()), 0) << customer->name();
  customers_[customer->name()] = std::make_pair(customer, false);
}

Customer* Manager::customer(const string& name) {
  auto it = customers_.find(name);
  if (it == customers_.end()) return nullptr;
  return it->second.first;
}

void Manager::removeCustomer(const string& name) {
  auto it = customers_.find(name);
  if (it == customers_.end()) return;
  customers_.erase(it);
}

void Manager::waitServersReady() {
  while (num_servers_ < FLAGS_num_servers) usleep(500);
}
void Manager::waitWorkersReady() {
  while (num_workers_ < FLAGS_num_workers) usleep(500);
}

} // namespace PS
