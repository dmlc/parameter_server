#pragma once
#include "util/common.h"
#include "system/proto/node.pb.h"
#include "system/proto/task.pb.h"
#include "system/van.h"
#include "system/assigner.h"
namespace PS {

DECLARE_int32(num_servers);
DECLARE_int32(num_workers);
DECLARE_int32(num_replicas);

class App;
class Customer;

// TODO

class Manager {
 public:
  Manager();
  ~Manager();

  void init();
  void run();
  void stop();
  bool process(const MessagePtr& msg);

  // manage nodes
  void addNode(const Node& node);
  void removeNode(const Node& node);

  // manage customer
  Customer* customer(const string& name);
  void addCustomer(Customer* obj);
  void removeCustomer(const string& name);

  int numWorkers() { return num_workers_; }
  int numServers() { return num_servers_; }
  Van& van() { return van_; }
  App* app() { return app_; }
 private:
  DISALLOW_COPY_AND_ASSIGN(Manager);
  bool isScheduler() { return van_.myNode().role() == Node::SCHEDULER; }
  Task newControlTask(Control::Command cmd, const string& customer_id = "");
  void sendTask(const Node& recver, const Task& task);
  void createApp(const string& name, const string& conf);

  App* app_ = nullptr;
  string app_conf_;
  std::promise<void> app_promise_;

  std::map<NodeID, Node> nodes_;
  std::map<NodeID, Node> new_nodes_;

  bool done_ = false;

  NodeAssigner* node_assigner_ = nullptr;

  std::map<string, std::pair<Customer*, bool>> customers_;

  Van van_;
  int num_workers_ = 0;
  int num_servers_ = 0;

  int num_active_nodes_ = 0;
};

} // namespace PS
