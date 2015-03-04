#include "system/manager.h"
#include "system/postoffice.h"
#include "system/app.h"
#include "system/assigner.h"
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
  node_assigner_ = new NodeAssigner();
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


} // namespace PS

// #include "system/yellow_pages.h"
// #include "system/dashboard.h"

  // yp_ should stay behind sending_queue_ so it will be destroied earlier
  // YellowPages yellow_pages_;

  // Postmaster master_;
  // App* app_ = nullptr;
  // std::promise<void> app_promise_;
  // MessagePtr app_msg_;

  // std::mutex mutex_;
  // bool done_ = false;

  // int finished_nodes_ = 0;
  // std::promise<void> nodes_are_done_;

  // std::promise<void> nodes_are_ready_;
  // std::promise<void> run_app_promise_;

  // void createApp(const string& name, const string& conf);
  // void manageApp(MessagePtr msg);
  // void finish(MessagePtr msg);
  // // return the running app and its conf
  // App* app() { return CHECK_NOTNULL(app_); }
  // // std::string appConf() { return app_conf_; }

  // YellowPages& yp() { return yellow_pages_; }
  // Node& myNode() { return yellow_pages_.van().myNode(); }
  // Node& scheduler() { return yellow_pages_.van().scheduler(); }

  // HeartbeatInfo& hb() { return heartbeat_info_; };

  // string printDashboardTitle();
  // string printHeartbeatReport(const string& node_id, const HeartbeatReport& report);
  // std::unique_ptr<std::thread> heartbeating_;
  // std::unique_ptr<std::thread> monitoring_;
  // // heartbeat thread function
  // void heartbeat();
  // // monitor thread function only used by scheduler
  // void monitor();
  // // heartbeat info for workers/servers
  // HeartbeatInfo heartbeat_info_;
  // Dashboard dashboard_;
// heartbeat_info_.
// if (FLAGS_report_interval > 0 && Node::SCHEDULER == myNode().role()) {
//   dashboard_.addTask(msg->sender, msg->task.time());
// }




// void Postoffice::finish(MessagePtr msg) {
//   if (msg->finished) {
//     auto obj = CHECK_NOTNULL(yp().customer(msg->task.customer()));
//     obj->exec().finish(msg);
//     reply(msg->sender, msg->task);
//   }
// }

//   if (isScheduler()) {
//     CHECK_GT(FLAGS_num_servers, 0);
//     CHECK_GT(FLAGS_num_workers, 0);

//     // add my node into app_
//     Task task;
//     task.set_request(true);
//     task.set_customer(app_->name());
//     task.set_type(Task::MANAGE_NODE);
//     task.mutable_mng_node()->set_cmd(ManageNode::ADD);
//     *task.mutable_mng_node()->add_node() = myNode();
//     node_manager_.process(task);

//     // init other nodes
//     if (FLAGS_num_workers + FLAGS_num_servers) {
//       nodes_are_ready_.get_future().wait();
//       LI << "Scheduler has connected " << FLAGS_num_servers << " servers and "
//          << FLAGS_num_workers << " workers";
//      // wait until app has been created at all computation nodes
//       app_->port(kCompGroup)->waitOutgoingTask(1);

//       // add all nodes into app
//       auto nodes = yp().nodes();
//       nodes = Postmaster::partitionServerKeyRange(nodes, Range<Key>::all());
//       nodes = Postmaster::assignNodeRank(nodes);
//       Task task;
//       // task.set_request(true);
//       // task.set_customer(app_->name());
//       task.set_type(Task::MANAGE);
//       task.mutable_mng_node()->set_cmd(ManageNode::ADD);
//       for (const auto& n : nodes) {
//         *task.mutable_mng_node()->add_node() = n;
//       }
//       // then add them in servers and worekrs
//       app_->port(kCompGroup)->submitAndWait(task);

//       // init app
//       Task init;
//       init.set_type(Task::MANAGE);
//       init.mutable_mng_app()->set_cmd(ManageApp::INIT);
//       int init_wait = app_->port(kCompGroup)->submit(init);
//       CHECK_NOTNULL(app_)->init();
//       app_->port(kCompGroup)->waitOutgoingTask(init_wait);

//       // run app
//       Task run;
//       run.set_type(Task::MANAGE);
//       run.mutable_mng_app()->set_cmd(ManageApp::RUN);
//       int run_wait = app_->port(kCompGroup)->submit(run);
//       app_->run();
//       app_->port(kCompGroup)->waitOutgoingTask(run_wait);
//     } else {
//       CHECK_NOTNULL(app_)->init();
//       app_->run();
//     }
//   } else {
//     // init app
//     init_app_promise_.get_future().wait();
//     CHECK_NOTNULL(app_)->init();
//     app_msg_->finished = true;
//     finish(app_msg_);

//     // run app
//     run_app_promise_.get_future().wait();
//     CHECK_NOTNULL(app_)->run();
//     app_msg_->finished = true;
//     finish(app_msg_);


//   }

//   app_promise_.get_future().wait();
//   app_->run();
// }

// void Postoffice::stop() {
//   manager_.stop();
// }
//   if (isScheduler()) {
//     if (FLAGS_num_workers + FLAGS_num_servers) {
//       nodes_are_done_.get_future().wait();
//     }
//     Task terminate;
//     terminate.set_type(Task::TERMINATE);
//     app_->port(kLiveGroup)->submit(terminate);
//     usleep(800);
//     LI << "System stopped\n";
//   } else {
//     Task done;
//     done.set_type(Task::MANAGE);
//     done.mutable_mng_app()->set_cmd(ManageApp::DONE);
//     app_->port(app_->schedulerID())->submit(done);
//     // run as a daemon until received the termination messag
//     while (!done_) usleep(300);
//   }
// }

// void Postoffice::manageApp(MessagePtr msg) {
//   Task tk = msg->task;
//   CHECK(tk.has_mng_app());
//   auto cmd = tk.mng_app().cmd();
//   if (cmd == ManageApp::ADD) {
//     app_ = App::create(tk.customer(), tk.mng_app().conf());
//     // yp().depositCustomer(app->name());
//   } else if (cmd == ManageApp::INIT) {
//     msg->finished = false;
//     if (!app_msg_) app_msg_ = MessagePtr(new Message());
//     *app_msg_ = *msg;
//     init_app_promise_.set_value();
//   } else if (cmd == ManageApp::RUN) {
//     msg->finished = false;
//     if (!app_msg_) app_msg_ = MessagePtr(new Message());
//     *app_msg_ = *msg;
//     run_app_promise_.set_value();
//   } else if (cmd == ManageApp::DONE) {
//     finished_nodes_ ++;
//     if (finished_nodes_ >= FLAGS_num_workers + FLAGS_num_servers) {
//       nodes_are_done_.set_value();
//     }
//   }
// }

// in run():
  // heartbeat_info_.init(FLAGS_interface, myNode().hostname());
  // // threads on statistic
  // if (FLAGS_report_interval > 0) {
  //   if (Node::SCHEDULER == myNode().role()) {
  //     monitoring_ = std::unique_ptr<std::thread>(
  //       new std::thread(&Postoffice::monitor, this));
  //     monitoring_->detach();
  //   } else {
  //     heartbeating_ = std::unique_ptr<std::thread>(
  //       new std::thread(&Postoffice::heartbeat, this));
  //     heartbeating_->detach();
  //   }
  // }
// void Postoffice::heartbeat() {
//   while (!done_) {
//     // heartbeat won't work until I have connected to the scheduler
//     std::this_thread::sleep_for(std::chrono::seconds(FLAGS_report_interval));
//     if (yp().van().connected(scheduler())) {
//       // serialize heartbeat report
//       string report;
//       heartbeat_info_.get().SerializeToString(&report);

//       // pack msg
//       Task task;
//       task.set_type(Task::HEARTBEATING);
//       task.set_request(true);
//       task.set_msg(report);
//       task.set_do_not_reply(true);
//       MessagePtr msg(new Message(task));
//       msg->recver = scheduler().id();

//       // push into send_thread queue
//       queue(msg);
//     }
//   }
// }

// void Postoffice::monitor() {
//   while (!done_) {
//     std::cerr << dashboard_.report() << "\n\n";
//     std::this_thread::sleep_for(std::chrono::seconds(FLAGS_report_interval));
//   }
// }
