#include "system/postoffice.h"
#include "proto/heartbeat.pb.h"
// #include <omp.h>
#include "system/customer.h"
#include "system/app.h"
#include "util/file.h"

namespace PS {

DEFINE_bool(enable_fault_tolerance, false, "enable fault tolerance feature");
DEFINE_int32(num_replicas, 0, "number of replica");
DEFINE_int32(num_servers, 0, "number of servers");
DEFINE_int32(num_workers, 0, "number of clients");
DEFINE_int32(num_unused, 0, "number of unused nodes");
DEFINE_int32(num_threads, 2, "number of computational threads");
DEFINE_string(app, "../config/rcv1/batch_l1lr.conf", "the configuration file of app");
DECLARE_string(interface);
DEFINE_bool(traffic_statistics, false, "print traffic statistic at the end");

// TODO move to configure
DEFINE_int32(report_interval, 0,
  "Servers/Workers report running status to scheduler "
  "in every report_interval seconds. "
  "default: 0; if set to 0, heartbeat is disabled");
DEFINE_bool(verbose, false, "print extra debug info");
DEFINE_bool(log_to_file, false, "redirect INFO log to file; eg. log_w1_datetime");

Postoffice::~Postoffice() {
  recving_->join();
  MessagePtr stop(new Message()); stop->terminate = true; queue(stop);
  sending_->join();
}

// TODO run a console if it is a Node::MANAGER
void Postoffice::run() {
  // omp_set_dynamic(0);
  // omp_set_num_threads(FLAGS_num_threads);
  yellow_pages_.init();
  heartbeat_info_.init(FLAGS_interface, myNode().hostname());

  if (FLAGS_log_to_file) {
    google::SetLogDestination(google::INFO, ("./log_" + myNode().id() + "_").c_str());
    FLAGS_logtostderr = 0;
  }

  recving_ = std::unique_ptr<std::thread>(new std::thread(&Postoffice::recv, this));
  sending_ = std::unique_ptr<std::thread>(new std::thread(&Postoffice::send, this));

  // threads on statistic
  if (FLAGS_report_interval > 0) {
    if (Node::SCHEDULER == myNode().role()) {
      monitoring_ = std::unique_ptr<std::thread>(
        new std::thread(&Postoffice::monitor, this));
      monitoring_->detach();
    } else {
      heartbeating_ = std::unique_ptr<std::thread>(
        new std::thread(&Postoffice::heartbeat, this));
      heartbeating_->detach();
    }
  }

  if (myNode().role() == Node::SCHEDULER) {
    // get all node information
    yellow_pages_.add(myNode());
    if (FLAGS_num_workers || FLAGS_num_servers) {
      nodes_are_ready_.get_future().wait();
      LI << "Scheduler connected " << FLAGS_num_servers << " servers and "
         << FLAGS_num_workers << " workers";
    }

    // run the application
    AppConfig conf; readFileToProtoOrDie(FLAGS_app, &conf);
    AppPtr app = App::create(conf);
    yellow_pages_.add(app);
    app->run();
    app->stopAll();
  } else {
    // sent my node info to the scheduler
    Task task;
    task.set_type(Task::MANAGE);
    task.set_request(true);
    task.set_do_not_reply(true);
    auto mng_node = task.mutable_mng_node();
    mng_node->set_cmd(ManageNode::ADD);
    *(mng_node->add_node()) = myNode();
    MessagePtr msg(new Message(task));
    msg->recver = scheduler().id();
    queue(msg);

    // run as a daemon
    while (!done_) usleep(300);
  }
}

void Postoffice::reply(
    const NodeID& recver, const Task& task, const string& reply_msg) {
  if (!task.request() || task.do_not_reply()) return;
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
    yellow_pages_.customer(tk.customer())->exec().accept(reply);
  }
}

//  TODO fault tolerance, check if node info has been changed
void Postoffice::send() {
  MessagePtr msg;
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg->terminate) break;
    size_t send_bytes = 0;
    Status stat = yellow_pages_.van().send(msg, &send_bytes);
    if (!stat.ok()) {
      LL << "sending " << *msg << " failed. error: " << stat.ToString();
    }
    heartbeat_info_.increaseOutBytes(send_bytes);
  }
}

void Postoffice::recv() {
  while (true) {
    // receive a message
    MessagePtr msg(new Message());
    size_t recv_bytes = 0;
    auto stat = yellow_pages_.van().recv(msg, &recv_bytes);
    CHECK(stat.ok()) << stat.ToString();
    heartbeat_info_.increaseInBytes(recv_bytes);

    // process it
    auto& tk = msg->task;
    bool request = tk.request();
    auto type = tk.type();
    if (type == Task::CALL_CUSTOMER || type == Task::REPLY) {
      auto pt = yellow_pages_.customer(tk.customer());
      CHECK(pt) << "customer [" << tk.customer() << "] doesn't exist";
      pt->exec().accept(msg);

      // if I am the scheduler,
      //   I also record the latest task id for W/S without extra trouble
      if (FLAGS_report_interval > 0 && Node::SCHEDULER == myNode().role()) {
        dashboard_.addTask(msg->sender, msg->task.time());
      }
      continue;
    }

    if (type == Task::HEARTBEATING) {
      // newly arrived heartbeat pack
      dashboard_.addReport(msg->sender, tk.msg());
    } else if (type == Task::MANAGE) {
      if (request && tk.has_mng_app()) manageApp(tk);
      if (request && tk.has_mng_node()) manageNode(tk);
    } else if (type == Task::TERMINATE) {
      if (FLAGS_traffic_statistics) {
        yellow_pages_.van().statistic();
      }
      done_ = true;
      break;
    }
    auto ptr = yellow_pages_.customer(tk.customer());
    if (ptr != nullptr) ptr->exec().finish(msg);
    reply(msg->sender, msg->task);
  }
}

void Postoffice::manageApp(const Task& tk) {
  CHECK(tk.has_mng_app());
  auto& mng = tk.mng_app();
  if (mng.cmd() == ManageApp::ADD) {
    yellow_pages_.add(std::static_pointer_cast<Customer>(App::create(mng.app_config())));
  }
}

void Postoffice::manageNode(const Task& tk) {

  CHECK(tk.has_mng_node());
  auto& mng = tk.mng_node();
  std::vector<Node> nodes;
  for (int i = 0; i < mng.node_size(); ++i) {
    nodes.push_back(mng.node(i));
  }
  auto obj = yellow_pages_.customer(tk.customer());
  switch (mng.cmd()) {
    case ManageNode::ADD:
      for (auto n : nodes) yellow_pages_.add(n);
      if (yellow_pages_.num_workers() >= FLAGS_num_workers &&
          yellow_pages_.num_servers() >= FLAGS_num_servers) {
        nodes_are_ready_.set_value();
      }
      break;
    case ManageNode::INIT:
      for (auto n : nodes) yellow_pages_.add(n);
      if (obj != nullptr) {
        obj->exec().init(nodes);
        for (auto c : obj->children()) {
          auto child = yellow_pages_.customer(c);
          if (child) child->exec().init(nodes);
        }
      }
      break;
    case ManageNode::REPLACE:
      CHECK_EQ(nodes.size(), 2);
      obj->exec().replace(nodes[0], nodes[1]);
      for (auto c : obj->children())
        yellow_pages_.customer(c)->exec().replace(nodes[0], nodes[1]);
      break;

    default:
      CHECK(false) << " unknow command " << mng.cmd();
  }
}

void Postoffice::heartbeat() {
  while (!done_) {
    // heartbeat won't work until I have connected to the scheduler
    std::this_thread::sleep_for(std::chrono::seconds(FLAGS_report_interval));
    if (yellow_pages_.van().connected(scheduler())) {
      // serialize heartbeat report
      string report;
      heartbeat_info_.get().SerializeToString(&report);

      // pack msg
      Task task;
      task.set_type(Task::HEARTBEATING);
      task.set_request(true);
      task.set_msg(report);
      task.set_do_not_reply(true);
      MessagePtr msg(new Message(task));
      msg->recver = scheduler().id();

      // push into sending queue
      queue(msg);
    }
  }
}

void Postoffice::monitor() {
  while (!done_) {
    std::cerr << dashboard_.report() << "\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(FLAGS_report_interval));
  }
}

} // namespace PS
