#pragma once
#include "util/common.h"
#include "system/proto/node.pb.h"
#include "system/proto/task.pb.h"
#include "system/van.h"
#include "system/env.h"
#include "system/assigner.h"
namespace PS {

class App;
class Customer;

class Manager {
 public:
  Manager();
  ~Manager();

  void Init(char* argv0);
  void Run();
  void Stop();
  bool Process(Message* msg);

  // manage nodes
  void AddNode(const Node& node);
  void RemoveNode(const NodeID& node_id);
  // detect that *node_id* is disconnected
  void NodeDisconnected(const NodeID node_id);
  // add a function handler which will be called in *nodeDisconnected*
  typedef std::function<void(const NodeID&)> NodeFailureHandler;
  void AddNodeFailureHandler(NodeFailureHandler handler) {
    node_failure_handlers_.push_back(handler);
  }

  // manage customer
  Customer* customer(int id);
  void AddCustomer(Customer* obj);
  void RemoveCustomer(int id);
  int NextCustomerID();

  // workers and servers
  void WaitServersReady();
  void WaitWorkersReady();

  int num_workers() { return num_workers_; }
  int num_servers() { return num_servers_; }

  // manage message TODO
  void AddRequest(Message* msg) { delete msg; }
  void AddResponse(Message* msg) { }

  // accessors
  Van& van() { return van_; }
  App* app() { return app_; }

 private:
  bool IsScheduler() { return van_.my_node().role() == Node::SCHEDULER; }
  Task NewControlTask(Control::Command cmd);
  void SendTask(const NodeID& recver, const Task& task);
  void SendTask(const Node& recver, const Task& task) {
    SendTask(recver.id(), task);
  }

  // the app
  void CreateApp(const string& conf);
  App* app_ = nullptr;
  string app_conf_;
  // std::promise<void> my_node_promise_;

  // nodes
  std::map<NodeID, Node> nodes_;
  std::mutex nodes_mu_;
  int num_workers_ = 0;
  int num_servers_ = 0;
  int num_active_nodes_ = 0;
  std::vector<NodeFailureHandler> node_failure_handlers_;
  bool is_my_node_inited_ = false;

  // only available at the scheduler node
  NodeAssigner* node_assigner_ = nullptr;

  // customers
  // format: <id, <obj_ptr, is_deletable>>
  std::map<int, std::pair<Customer*, bool>> customers_;

  bool done_ = false;
  bool in_exit_ = false;
  int time_ = 0;

  Van van_;
  Env env_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

} // namespace PS
