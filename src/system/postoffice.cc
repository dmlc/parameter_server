#include "system/postoffice.h"
#include "proto/heartbeat.pb.h"
// #include <omp.h>
#include "system/customer.h"
#include "system/postmaster.h"
#include "system/app.h"
#include "util/file.h"

namespace PS {

DEFINE_int32(num_servers, 0, "number of servers");
DEFINE_int32(num_workers, 0, "number of clients");
DEFINE_int32(num_unused, 0, "number of unused nodes");
DEFINE_int32(num_threads, 2, "number of computational threads");
// DEFINE_string(app, "", "the configuration file of app");

DEFINE_string(app_name, "app", "the name of this app");
DEFINE_string(app_conf, "", "the string configuration of app");
DEFINE_string(app_file, "", "the configuration file of app");

DECLARE_string(interface);
DEFINE_bool(traffic_statistics, false, "print traffic statistic at the end");


DEFINE_int32(report_interval, 0,
  "Servers/Workers report running status to scheduler "
  "in every report_interval seconds. "
  "default: 0; if set to 0, heartbeat is disabled");
DEFINE_bool(verbose, false, "print extra debug info");
DEFINE_bool(log_to_file, false, "redirect INFO log to file; eg. log_w1_datetime");
DEFINE_bool(enable_fault_tolerance, false, "enable fault tolerance feature");
DEFINE_int32(num_replicas, 0, "number of replica");

Postoffice::Postoffice() {}

Postoffice::~Postoffice() {
  if (recving_) recving_->join();
  if (sending_) {
    MessagePtr stop(new Message()); stop->terminate = true; queue(stop);
    sending_->join();
  }
  delete app_;
}

void Postoffice::start(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_log_to_file) {
    google::SetLogDestination(
        google::INFO, ("./log_" + myNode().id() + "_").c_str());
    FLAGS_logtostderr = 0;
  } else {
    FLAGS_logtostderr = 1;
  }
  FLAGS_logbuflevel = -1;

  yp().init();
  // omp_set_dynamic(0);
  // omp_set_num_threads(FLAGS_num_threads);

  if (IamScheduler()) {
    // create the app
    if (!FLAGS_app_file.empty()) {
      CHECK(readFileToString(FLAGS_app_file, &app_conf_))
          << " failed to read conf file " << FLAGS_app_file;
    } else {
      app_conf_ = FLAGS_app_conf;
    }
    app_ = App::create(FLAGS_app_name, app_conf_);
    CHECK(app_ != NULL) << ": failed to create [" << FLAGS_app_name << "] with conf\n" << app_conf_;
  } else {
    // connect to the scheduler, which will send back a create_app request
    Task task;
    task.set_type(Task::MANAGE);
    task.set_request(true);
    task.set_time(0);
    auto mng_node = task.mutable_mng_node();
    mng_node->set_cmd(ManageNode::CONNECT);
    *(mng_node->add_node()) = myNode();
    MessagePtr msg(new Message(task));
    msg->recver = scheduler().id();
    queue(msg);
  }

  // start the I/O threads
  recving_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::recv, this));
  sending_ =
      std::unique_ptr<std::thread>(new std::thread(&Postoffice::send, this));

  if (IamScheduler()) {
    // add my node into app_
    Task task;
    task.set_request(true);
    task.set_customer(app_->name());
    task.set_type(Task::MANAGE);
    task.mutable_mng_node()->set_cmd(ManageNode::ADD);
    *task.mutable_mng_node()->add_node() = myNode();
    manageNode(task);

    // init other nodes
    if (FLAGS_num_workers + FLAGS_num_servers) {
      nodes_are_ready_.get_future().wait();
      LI << "Scheduler has connected " << FLAGS_num_servers << " servers and "
         << FLAGS_num_workers << " workers";
      // wait until app has been created at all computation nodes
      app_->port(kCompGroup)->waitOutgoingTask(1);

      // add all nodes into app
      auto nodes = yp().nodes();
      nodes = Postmaster::partitionServerKeyRange(nodes, Range<Key>::all());
      nodes = Postmaster::assignNodeRank(nodes);
      Task task;
      // task.set_request(true);
      // task.set_customer(app_->name());
      task.set_type(Task::MANAGE);
      task.mutable_mng_node()->set_cmd(ManageNode::ADD);
      for (const auto& n : nodes) {
        *task.mutable_mng_node()->add_node() = n;
      }
      // then add them in servers and worekrs
      app_->port(kCompGroup)->submitAndWait(task);

      // init app
      Task init;
      init.set_type(Task::MANAGE);
      init.mutable_mng_app()->set_cmd(ManageApp::INIT);
      int init_wait = app_->port(kCompGroup)->submit(init);
      CHECK_NOTNULL(app_)->init();
      app_->port(kCompGroup)->waitOutgoingTask(init_wait);

      // run app
      Task run;
      run.set_type(Task::MANAGE);
      run.mutable_mng_app()->set_cmd(ManageApp::RUN);
      int run_wait = app_->port(kCompGroup)->submit(run);
      app_->run();
      app_->port(kCompGroup)->waitOutgoingTask(run_wait);
    } else {
      CHECK_NOTNULL(app_)->init();
      app_->run();
    }
  } else {
    // init app
    init_app_promise_.get_future().wait();
    CHECK_NOTNULL(app_)->init();
    app_msg_->finished = true;
    finish(app_msg_);

    // run app
    run_app_promise_.get_future().wait();
    CHECK_NOTNULL(app_)->run();
    app_msg_->finished = true;
    finish(app_msg_);


  }
}

void Postoffice::stop() {
  if (IamScheduler()) {
    if (FLAGS_num_workers + FLAGS_num_servers) {
      nodes_are_done_.get_future().wait();
    }
    Task terminate;
    terminate.set_type(Task::TERMINATE);
    app_->port(kLiveGroup)->submit(terminate);
    usleep(800);
    LI << "System stopped\n";
  } else {
    Task done;
    done.set_type(Task::MANAGE);
    done.mutable_mng_app()->set_cmd(ManageApp::DONE);
    app_->port(app_->schedulerID())->submit(done);
    // run as a daemon until received the termination messag
    while (!done_) usleep(300);
  }
}

void Postoffice::reply(
    const NodeID& recver, const Task& task, const string& reply_msg) {
  if (!task.request()) return;
  Task tk;
  tk.set_customer(task.customer());
  tk.set_request(false);
  tk.set_type(Task::REPLY);
  if (!reply_msg.empty()) tk.set_msg(reply_msg);
  tk.set_time(task.time());
  MessagePtr re(new Message(tk)); re->recver = recver; queue(re);
}

void Postoffice::queue(const MessagePtr& msg) {
  if (msg->valid) {
    sending_queue_.push(msg);
  } else {
    // do not send, fake a reply mesage
    Task tk;
    tk.set_customer(msg->task.customer());
    tk.set_request(false);
    tk.set_type(Task::REPLY);
    tk.set_time(msg->task.time());
    MessagePtr reply(new Message(tk));
    reply->sender = msg->recver;
    reply->recver = msg->sender;
    yp().customer(tk.customer())->exec().accept(reply);
  }
}

//  TODO fault tolerance, check if node info has been changed
void Postoffice::send() {
  MessagePtr msg;
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg->terminate) break;
    size_t send_bytes = 0;
    Status stat = yp().van().send(msg, &send_bytes);
    if (!stat.ok()) {
      LL << "sending " << *msg << " failed. error: " << stat.ToString();
    }
    // heartbeat_info_.increaseOutBytes(send_bytes);
  }
}

void Postoffice::recv() {
  while (true) {
    // receive a message
    MessagePtr msg(new Message());
    size_t recv_bytes = 0;
    auto stat = yp().van().recv(msg, &recv_bytes);
    CHECK(stat.ok()) << stat.ToString();
    // heartbeat_info_.increaseInBytes(recv_bytes);

    // process it
    auto& tk = msg->task;
    bool request = tk.request();
    auto type = tk.type();
    if (type == Task::CALL_CUSTOMER || type == Task::REPLY) {
      auto pt = yp().customer(tk.customer());
      CHECK(pt) << "customer [" << tk.customer() << "] doesn't exist";
      pt->exec().accept(msg);

      // if (FLAGS_report_interval > 0 && Node::SCHEDULER == myNode().role()) {
      //   dashboard_.addTask(msg->sender, msg->task.time());
      // }
      continue;
    }

    if (type == Task::HEARTBEATING) {
      // dashboard_.addReport(msg->sender, tk.msg());
    } else if (type == Task::MANAGE) {
      if (request && tk.has_mng_app()) {
        manageApp(msg);
      }
      if (request && tk.has_mng_node()) {
        manageNode(tk);
      }
    } else if (type == Task::TERMINATE) {
      if (FLAGS_traffic_statistics) {
        yp().van().statistic();
      }
      done_ = true;
      break;
    }
    finish(msg);
  }
}

void Postoffice::finish(MessagePtr msg) {
  if (msg->finished) {
    auto obj = CHECK_NOTNULL(yp().customer(msg->task.customer()));
    obj->exec().finish(msg);
    reply(msg->sender, msg->task);
  }
}

void Postoffice::manageApp(MessagePtr msg) {
  Task tk = msg->task;
  CHECK(tk.has_mng_app());
  auto cmd = tk.mng_app().cmd();
  if (cmd == ManageApp::ADD) {
    app_ = App::create(tk.customer(), tk.mng_app().conf());
    // yp().depositCustomer(app->name());
  } else if (cmd == ManageApp::INIT) {
    msg->finished = false;
    if (!app_msg_) app_msg_ = MessagePtr(new Message());
    *app_msg_ = *msg;
    init_app_promise_.set_value();
  } else if (cmd == ManageApp::RUN) {
    msg->finished = false;
    if (!app_msg_) app_msg_ = MessagePtr(new Message());
    *app_msg_ = *msg;
    run_app_promise_.set_value();
  } else if (cmd == ManageApp::DONE) {
    finished_nodes_ ++;
    if (finished_nodes_ >= FLAGS_num_workers + FLAGS_num_servers) {
      nodes_are_done_.set_value();
    }
  }
}

void Postoffice::manageNode(Task& tk) {
  CHECK(tk.has_mng_node());
  auto& mng = tk.mng_node();
  if (mng.cmd() == ManageNode::CONNECT) {
    CHECK(IamScheduler());
    CHECK_EQ(mng.node_size(), 1);
    // first add this node into app
    Task add = tk;
    add.set_customer(CHECK_NOTNULL(app_)->name());
    add.mutable_mng_node()->set_cmd(ManageNode::ADD);
    manageNode(add);
    // create the app in this node
    Task task;
    task.set_request(true);
    task.set_customer(app_->name());
    task.set_type(Task::MANAGE);
    task.set_time(1);
    task.mutable_mng_app()->set_cmd(ManageApp::ADD);
    task.mutable_mng_app()->set_conf(app_conf_);
    app_->port(mng.node(0).id())->submit(task);
    // check if all nodes are connected
    if (yp().num_workers() >= FLAGS_num_workers &&
        yp().num_servers() >= FLAGS_num_servers) {
      nodes_are_ready_.set_value();
    }
    tk.set_customer(app_->name());  // otherwise the remote node doesn't know
                                    // how to find the according customer
  } else if (mng.cmd() == ManageNode::ADD) {
    auto obj = yp().customer(tk.customer());
    CHECK(obj) << "customer [" << tk.customer() << "] doesn't exists";
    for (int i = 0; i < mng.node_size(); ++i) {
      auto node = mng.node(i);
      yp().addNode(node);
      obj->exec().add(node);
      for (auto c : yp().childern(obj->name())) {
        auto child = yp().customer(c);
        if (child) child->exec().add(node);
      }
    }
  } else if (mng.cmd() == ManageNode::REPLACE) {
    // CHECK_EQ(nodes.size(), 2);
    // obj->exec().replace(nodes[0], nodes[1]);
    // for (auto c : obj->children())
    //   yp().customer(c)->exec().replace(nodes[0], nodes[1]);
    // break;
  } else if (mng.cmd() == ManageNode::REMOVE) {
  }
}

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

//       // push into sending queue
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

} // namespace PS
