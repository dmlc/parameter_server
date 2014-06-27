#include "app/app.h"
#include "app/linear_block_iterator.h"
#include "app/block_coordinate_l1lr.h"

// #include "app/grad_desc.h"
// #include "app/block_prox_grad.h"
// #include "app/sketch.h"

namespace PS {

DEFINE_bool(test_fault_tol, false, "");

AppPtr App::create(const AppConfig& config) {
  AppPtr ptr;
  if (config.has_block_coord_l1lr()) {
    ptr = AppPtr(new BlockCoordinateL1LR());
  } else if (config.has_block_iterator()) {
    ptr = AppPtr(new LinearBlockIterator());
  } else {
    CHECK(false) << "unknown app: " << config.DebugString();
  }
  ptr->set(config);
  ptr->init();
  return ptr;
}

void App::stop() {
  Task terminate;
  terminate.set_type(Task::TERMINATE);
  taskpool(kLiveGroup)->submit(terminate);
  // terminate.set_type(Task::TERMINATE_CONFIRM);
  usleep(800);
  fprintf(stderr, "system stopped\n");
}

void App::requestNodes() {
  std::ifstream in(FLAGS_node_file);
  CHECK(in.good()) << "fail to read " << FLAGS_node_file;
  // int c = 0, s = 0;
  nodes_.clear();
  string str;
  while (in) {
    std::getline(in, str);
    if (str.empty()) continue;
    auto node = Van::parseNode(str);
    nodes_[node.id()] = node;
  }
}

void App::testFaultTolerance(Task recover) {
  CHECK_GT(FLAGS_num_replicas, 0);

  // TODO
  // // terminate s0
  // auto& s0 = nodes_["S0"];
  // auto ts0 = taskpool("S0");
  // Task terminate;
  // terminate.set_type(Task::TERMINATE);
  // ts0->submitAndWait(terminate);
  // terminate.set_type(Task::TERMINATE_CONFIRM);
  // ts0->submit(terminate);
  // LL << "S0 stopped";

  // // updates nodes_
  // LL << "start backup node U0";
  // CHECK_GT(FLAGS_num_unused, 0);
  // auto& u0 = nodes_["U0"];
  // u0.set_role(Node::SERVER);
  // *u0.mutable_key() = s0.key();
  // s0.set_role(Node::UNUSED);

  // // init U0
  // Task add = App::startNode();
  // auto tu0 = taskpool("U0");
  // tu0->submitAndWait(add);
  // LL << "sch: u0 inited";

  // // ask U0 doing recovering
  // recover.set_type(Task::CALL_CUSTOMER);
  // tu0->submitAndWait(recover);
  // LL << "sch: u0 recovered";

  // // ask rest nodes to updates their node info
  // auto all = taskpool(kActiveGroup);
  // Task replace;
  // replace.set_type(Task::MANAGE);
  // replace.set_customer(name_);
  // // replace.set_priority(1024);
  // replace.set_time(1000000);
  // auto cmd = replace.mutable_mng_node();
  // cmd->set_cmd(ManageNode::REPLACE);
  // *cmd->add_nodes() = s0;
  // *cmd->add_nodes() = u0;

  // LL << replace.DebugString();
  // // update myself, and then others
  // sys_.manage_node(replace);

  // taskpool(kActiveGroup)->submitAndWait(replace);

  // LL << "recovering is finished";
}

} // namespace PS
