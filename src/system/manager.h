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
  void removeNode(const NodeID& node_id);
  // detect that *node_id* is disconnected
  void nodeDisconnected(const NodeID node_id);
  // add a function handler which will be called in *nodeDisconnected*
  typedef std::function<void(const NodeID&)> NodeFailureHandler;
  void addNodeFailureHandler(NodeFailureHandler handler) {
    node_failure_handlers_.push_back(handler);
  }

  // manage customer
  Customer* customer(int id);
  void addCustomer(Customer* obj);
  void removeCustomer(int id);
  int nextCustomerID();

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

  // the app
  void createApp(const string& conf);
  App* app_ = nullptr;
  string app_conf_;
  std::promise<void> app_promise_;

  // nodes
  std::map<NodeID, Node> nodes_;
  int num_workers_ = 0;
  int num_servers_ = 0;
  int num_active_nodes_ = 0;
  std::vector<NodeFailureHandler> node_failure_handlers_;

  // only available at the scheduler node
  NodeAssigner* node_assigner_ = nullptr;

  // customers
  // format: <id, <obj_ptr, is_deletable>>
  std::map<int, std::pair<Customer*, bool>> customers_;

  bool done_ = false;
  bool in_exit_ = false;
  int time_ = 0;

  Van van_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

} // namespace PS
