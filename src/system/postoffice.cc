#include <iomanip>
#include "system/postoffice.h"
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
DEFINE_string(app, "../config/rcv1_l1lr.config", "the configuration file of app");
DEFINE_string(node_file, "./nodes", "node information");
DEFINE_int32(report_interval, 0,
  "Servers/Workers report running status to scheduler "
  "in every report_interval seconds. "
  "default: 0; if set to 0, heartbeat is disabled");
DEFINE_bool(verbose, false, "print extra debug info");
DEFINE_bool(log_to_file, false, "redirect INFO log to file; eg. log_w1_datetime");
DEFINE_bool(parallel_match, false, "enable multi-threaded match operation");
DECLARE_string(interface);

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

  switch(myNode().role()) {
    case Node::SCHEDULER: {
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
      break;
    } default:
      // run as a daemon
      addMyNode("default", yellow_pages_.van().scheduler());
      while (!done_) usleep(300);
      break;
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

void Postoffice::queue(const MessageCPtr& msg) {
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
  MessageCPtr msg;
  while (true) {
    sending_queue_.wait_and_pop(msg);
    if (msg->terminate) break;
    size_t send_bytes = 0;
    Status stat = yellow_pages_.van().send(msg, send_bytes);
    if (!stat.ok()) {
      LL << "sending " << msg->debugString() << " failed. error: " << stat.ToString();
    }
    heartbeat_info_.increaseOutBytes(send_bytes);
  }
}

void Postoffice::recv() {
  while (true) {
    MessagePtr msg(new Message());
    size_t recv_bytes = 0;
    auto stat = yellow_pages_.van().recv(msg, recv_bytes);
    CHECK(stat.ok()) << stat.ToString();
    heartbeat_info_.increaseInBytes(recv_bytes);

    auto& tk = msg->task;
    if (tk.request() && tk.type() == Task::TERMINATE) {
      // yellow_pages_.van().statistic();
      done_ = true;
      break;
    } else if (tk.request() && tk.type() == Task::MANAGE) {
      if (tk.has_mng_app()) manageApp(tk);
      if (tk.has_mng_node()) manageNode(tk);
    } else if (Task::HEARTBEATING == tk.type()) {
      // newly arrived heartbeat pack
      CHECK(tk.has_msg());
      HeartbeatReport report;
      report.ParseFromString(tk.msg());
      {
        Lock l(dashboard_mu_);
        report.set_task_id(dashboard_[msg->sender].task_id());
        dashboard_[msg->sender] = report;
      }
    } else {
      if (tk.customer() == "default") continue;
      auto pt = yellow_pages_.customer(tk.customer());
      CHECK(pt) << "customer [" << tk.customer() << "] doesn't exist";
      pt->exec().accept(msg);

      // if I am the scheduler,
      //   I also record the latest task id for W/S without extra trouble
      if (Node::SCHEDULER == myNode().role() && FLAGS_report_interval > 0) {
        Lock l(dashboard_mu_);
        dashboard_[msg->sender].set_task_id(msg->task.time());
      }

      continue;
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

void Postoffice::addMyNode(const string& name, const Node& recver) {
  Task task;
  task.set_request(true);
  task.set_customer(name);
  task.set_type(Task::MANAGE);
  auto mng_node = task.mutable_mng_node();
  mng_node->set_cmd(ManageNode::ADD);
  *(mng_node->add_node()) = myNode();

  MessagePtr msg(new Message(task));
  msg->recver = recver.id();
  queue(msg);
}

void Postoffice::heartbeat() {
  while (!done_) {
    // heartbeat won't work until I have connected to the scheduler
    if (yellow_pages_.van().connectivity("H").ok()) {
      // serialize heartbeat report
      string report;
      heartbeat_info_.get().SerializeToString(&report);

      // pack msg
      MessagePtr msg(new Message("H", 0));
      msg->sender = myNode().id();
      msg->valid = true;
      msg->task.set_type(Task::HEARTBEATING);
      msg->task.set_request(false);
      msg->task.set_customer("HB");
      msg->task.set_msg(report);

      // push into sending queue
      queue(msg);

      // take a rest
      std::this_thread::sleep_for(std::chrono::seconds(FLAGS_report_interval));
    }
  }
}

void Postoffice::monitor() {
  while (!done_) {
    {
      Lock l(dashboard_mu_);
      if (!dashboard_.empty()) {
        // prepare formatted dashboard
        std::stringstream ss;
        ss << printDashboardTitle() << "\n";
        for (const auto& report : dashboard_) {
          ss << printHeartbeatReport(report.first, report.second) << "\n";
        }

        // output
        std::cerr << "\n\n" << ss.str();
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(FLAGS_report_interval));
  }
}

string Postoffice::printDashboardTitle() {
  const size_t WIDTH = 10;

  // time_t
  std::time_t now_time = std::chrono::system_clock::to_time_t(
    std::chrono::system_clock::now());
  string time_str = ctime(&now_time);
  time_str.resize(time_str.size() - 1);

  std::stringstream ss;
  ss << std::setiosflags(std::ios::left) <<
    std::setw(WIDTH * 2) << std::setfill('=') << "" << " Dashboard " <<
    time_str + " " << std::setw(WIDTH * 2) << std::setfill('=') << "" << "\n";

  ss << std::setfill(' ') <<
    std::setw(WIDTH) << "Node" <<
    std::setw(WIDTH) << "Task" <<
    std::setw(WIDTH) << "MyCPU(%)" <<
    std::setw(WIDTH) << "MyRSS(M)" <<
    std::setw(WIDTH) << "MyVir(M)" <<
    std::setw(WIDTH) << "BusyTime" <<
    std::setw(WIDTH) << "InMB" <<
    std::setw(WIDTH) << "OutMB" <<
    std::setw(WIDTH) << "HostCPU" <<
    std::setw(WIDTH) << "HostUseGB" <<
    std::setw(WIDTH) << "HostInBW" <<
    std::setw(WIDTH) << "HostOutBW" <<
    std::setw(WIDTH * 2) << "HostName";

  return ss.str();
}

string Postoffice::printHeartbeatReport(
  const string& node_id,
  const HeartbeatReport& report) {
  std::stringstream ss;
  const size_t WIDTH = 10;

  std::stringstream busy_time_with_ratio;
  busy_time_with_ratio << report.busy_time_milli() <<
    "(" << static_cast<uint32>(
    100 * (static_cast<float>(report.busy_time_milli()) / report.total_time_milli())) <<
    "%)";

  std::stringstream net_in_mb_with_speed;
  net_in_mb_with_speed << report.net_in_mb() <<
    "(" << static_cast<uint32>(report.net_in_mb() / (report.total_time_milli() / 1e3)) <<
    ")";

  std::stringstream net_out_mb_with_speed;
  net_out_mb_with_speed << report.net_out_mb() <<
    "(" << static_cast<uint32>(report.net_out_mb() / (report.total_time_milli() / 1e3)) <<
    ")";

  std::stringstream host_memory_usage;
  host_memory_usage << report.host_in_use_gb() << "(" <<
    report.host_in_use_percentage() << "%)";

  ss << std::setiosflags(std::ios::left) <<
    std::setw(WIDTH) << node_id <<
    std::setw(WIDTH) << report.task_id() <<
    std::setw(WIDTH) << report.process_cpu_usage() <<
    std::setw(WIDTH) << report.process_rss_mb() <<
    std::setw(WIDTH) << report.process_virt_mb() <<
    std::setw(WIDTH) << busy_time_with_ratio.str() <<
    std::setw(WIDTH) << net_in_mb_with_speed.str() <<
    std::setw(WIDTH) << net_out_mb_with_speed.str() <<
    std::setw(WIDTH) << report.host_cpu_usage() <<
    std::setw(WIDTH) << host_memory_usage.str() <<
    std::setw(WIDTH) << (
      report.host_net_in_bw() < 1024 ? std::to_string(report.host_net_in_bw()) : "INIT") <<
    std::setw(WIDTH) << (
      report.host_net_out_bw() < 1024 ? std::to_string(report.host_net_out_bw()) : "INIT") <<
    std::setw(WIDTH * 2) << report.hostname();

  return ss.str();
}

} // namespace PS
