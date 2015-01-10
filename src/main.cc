#include "system/postoffice.h"
#include "data/show_example.h"
#include "data/text2proto.h"
#include "system/app.h"
#include "system/app_test.h"
// #include "linear_method/linear_method.h"
// #include "graph_partition/graph_partition.h"
// #include "factorization_machine/fm.h"
DEFINE_bool(log_instant, false, "disable buffer of glog");

namespace PS {
App* App::create(const AppConfig& conf) {
  App* ptr = nullptr;
  if (conf.has_linear_method()) {
  //   ptr = LM::LinearMethod::create(conf.app_name(), conf.linear_method());
  } else if (conf.has_graph_partition()) {
  //   ptr = GP::GraphPartition::create(conf.graph_partition());
  } else if (conf.has_factorization_machine()) {
  //   ptr = FM::FactorizationMachine::create(conf.factorization_machine());
  } else {
    ptr = new AppTest(conf.app_name());
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
