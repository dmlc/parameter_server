#include "system/postoffice.h"
// #include <omp.h>
#include "system/customer.h"
#include "system/app.h"
#include "util/file.h"

namespace PS {

DEFINE_bool(enable_fault_tolerance, false, "enable fault tolerance feature");
DEFINE_int32(num_replicas, 0, "number of replica");
DEFINE_int32(num_servers, 1, "number of servers");
DEFINE_int32(num_workers, 1, "number of clients");
DEFINE_int32(num_unused, 0, "number of unused nodes");
DEFINE_int32(num_threads, 2, "number of computational threads");
DEFINE_string(app, "../config/rcv1_l1lr.config", "the configuration file of app");
DEFINE_string(node_file, "./nodes", "node information");

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
  recving_ = std::unique_ptr<std::thread>(new std::thread(&Postoffice::recv, this));
  sending_ = std::unique_ptr<std::thread>(new std::thread(&Postoffice::send, this));
  switch(myNode().role()) {
    case Node::SCHEDULER: {
      // get all node information
      yellow_pages_.add(myNode());
      nodes_are_ready_.get_future().wait();
      LI << "Scheduler connected " << FLAGS_num_servers << " servers and "
         << FLAGS_num_workers << " workers";

      // run the application
      AppConfig conf; readFileToProtoOrDie(FLAGS_app, &conf);
      AppPtr app = App::create(conf);
      yellow_pages_.add(app);
      app->run();
      app->stop();
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
    Status stat = yellow_pages_.van().send(msg);
    if (!stat.ok()) {
      LL << "sending " << msg->debugString() << " failed. error: " << stat.ToString();
    }
  }
}

void Postoffice::recv() {
  while (true) {
    MessagePtr msg(new Message());
    auto stat = yellow_pages_.van().recv(msg);
    CHECK(stat.ok()) << stat.ToString();
    auto& tk = msg->task;
    if (tk.request() && tk.type() == Task::TERMINATE) {
      yellow_pages_.van().statistic();
      done_ = true;
      break;
    } else if (tk.request() && tk.type() == Task::MANAGE) {
      if (tk.has_mng_app()) manageApp(tk);
      if (tk.has_mng_node()) manageNode(tk);
    } else {
      if (tk.customer() == "default") continue;
      auto pt = yellow_pages_.customer(tk.customer());
      CHECK(pt) << "customer [" << tk.customer() << "] doesn't exist";
      pt->exec().accept(msg);
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
        for (auto c : obj->children())
          yellow_pages_.customer(c)->exec().init(nodes);
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

} // namespace PS
