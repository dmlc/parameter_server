#pragma once
#include "util/common.h"
#include "system/proto/node.pb.h"
#include "system/proto/task.pb.h"
#include "system/van.h"
#include "system/assigner.h"
namespace PS {

class App;
class Customer;

class Manager {
 public:
  Manager();
  ~Manager();

  void init(char* argv0);
  void run();
  void stop();
  bool process(const MessagePtr& msg);

  // manage nodes
  void addNode(const Node& node);
  void removeNode(const Node& node);

  // manage customer
  Customer* customer(int id) { CHECK(false); }  // should check
  void addCustomer(Customer* obj) { }
  void removeCustomer(int id) { }
  int nextCustomerID() { return 0; }

  // workers and servers
  void waitServersReady();
  void waitWorkersReady();

  int numWorkers() { return num_workers_; }
  int numServers() { return num_servers_; }

  // manage message
  void addPendingMsg(const MessagePtr& msg) { }
  void removePendingMsg(const MessagePtr& msg) { }

  // accessors
  Van& van() { return van_; }
  App* app() { return app_; }

 private:
  bool isScheduler() { return van_.myNode().role() == Node::SCHEDULER; }
  Task newControlTask(Control::Command cmd);
  void sendTask(const NodeID& recver, const Task& task);
  void sendTask(const Node& recver, const Task& task) {
    sendTask(recver.id(), task);
  }
  void createApp(const string& conf);


  App* app_ = nullptr;
  string app_conf_;
  std::promise<void> app_promise_;

  std::map<NodeID, Node> nodes_;
  int num_workers_ = 0;
  int num_servers_ = 0;
  int num_active_nodes_ = 0;

  bool done_ = false;

  // only available at the scheduler node
  NodeAssigner* node_assigner_ = nullptr;

  class NodeManager {
  };
  NodeManager node_manager_;

  class CustomerManager {
  };
  CustomerManager customer_manager_;

  std::map<int, std::pair<Customer*, bool>> customers_;

  Van van_;

  int time_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

} // namespace PS
