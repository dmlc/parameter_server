#include "system/app.h"
namespace PS {
DEFINE_bool(test_fault_tol, false, "");

void App::stopAll() {
  // send terminate signal to all others
  Task terminate;
  terminate.set_type(Task::TERMINATE);
  auto pool = taskpool(kLiveGroup);
  if (!pool) {
    // so it's a single machine version. i need to send the terminal signal to
    // myself
    std::vector<Node> nodes(1, sys_.myNode());
    exec_.init(nodes);
    pool = taskpool(kLiveGroup);
  }
  pool->submit(terminate);
  // terminate.set_type(Task::TERMINATE_CONFIRM);
  usleep(800);
  LI << "System stopped\n";
}

} // namespace PS

// void App::testFaultTolerance(Task recover) {
//   CHECK_GT(FLAGS_num_replicas, 0);
//   // terminate s0
//   auto& s0 = nodes_["S0"];
//   auto ts0 = taskpool("S0");
//   Task terminate;
//   terminate.set_type(Task::TERMINATE);
//   ts0->submitAndWait(terminate);
//   terminate.set_type(Task::TERMINATE_CONFIRM);
//   ts0->submit(terminate);
//   LL << "S0 stopped";

//   // updates nodes_
//   LL << "start backup node U0";
//   CHECK_GT(FLAGS_num_unused, 0);
//   auto& u0 = nodes_["U0"];
//   u0.set_role(Node::SERVER);
//   *u0.mutable_key() = s0.key();
//   s0.set_role(Node::UNUSED);

//   // init U0
//   Task add = App::startNode();
//   auto tu0 = taskpool("U0");
//   tu0->submitAndWait(add);
//   LL << "sch: u0 inited";

//   // ask U0 doing recovering
//   recover.set_type(Task::CALL_CUSTOMER);
//   tu0->submitAndWait(recover);
//   LL << "sch: u0 recovered";

//   // ask rest nodes to updates their node info
//   auto all = taskpool(kActiveGroup);
//   Task replace;
//   replace.set_type(Task::MANAGE);
//   replace.set_customer(name_);
//   // replace.set_priority(1024);
//   replace.set_time(1000000);
//   auto cmd = replace.mutable_mng_node();
//   cmd->set_cmd(ManageNode::REPLACE);
//   *cmd->add_nodes() = s0;
//   *cmd->add_nodes() = u0;

//   LL << replace.DebugString();
//   // update myself, and then others
//   sys_.manage_node(replace);

//   taskpool(kActiveGroup)->submitAndWait(replace);

//   LL << "recovering is finished";
// }
