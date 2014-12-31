#include "system/postoffice.h"
#include "data/show_example.h"
#include "data/text2proto.h"
#include "linear_method/linear_method.h"
#include "neural_network/sgd_solver.h"
#include "graph_partition/graph_partition.h"
DEFINE_bool(log_instant, false, "disable buffer of glog");

namespace PS {
AppPtr App::create(const AppConfig& conf) {
  AppPtr ptr;
  if (conf.has_linear_method()) {
    ptr = LM::LinearMethod::create(conf.linear_method());
    CHECK(ptr);
  } else if (conf.has_neural_network()) {
    ptr = AppPtr(new NN::SGDSolver());
  } else if (conf.has_graph_partition()) {
    ptr = GP::GraphPartition::create(conf.graph_partition());
  } else {
    CHECK(false) << "unknown app: " << conf.DebugString();
  }

  CHECK(conf.has_app_name());
  ptr->name_ = conf.app_name();
  for (int i = 0; i < conf.parameter_name_size(); ++i) {
    ptr->child_customers_.push_back(conf.parameter_name(i));
  }
  ptr->app_cf_ = conf;
  ptr->init();
  return ptr;
}
} // namespace PS

int main(int argc, char *argv[]) {
  using namespace PS;
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_logtostderr = 1;
  if (FLAGS_log_instant) FLAGS_logbuflevel = -1;

  if (FLAGS_app == "print_proto") {
    showExample();
  } else if (FLAGS_app == "text2proto") {
    textToProto();
  } else {
    PS::Postoffice::instance().run();
  }

  return 0;
}
