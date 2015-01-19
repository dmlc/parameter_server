#include "system/postoffice.h"
#include "google/protobuf/text_format.h"
// #include "data/show_example.h"
// #include "data/text2proto.h"
#include "system/app.h"
// #include "test/app_test.h"
#include "app/linear_method/linear.h"
#include "app/main/proto/app.pb.h"
// #include "app/graph_partition/graph_partition.h"
// #include "factorization_machine/fm.h"
// #include "cxxnet/cxxnet.h"
// DEFINE_bool(log_instant, false, "disable buffer of glog");

namespace PS {
App* App::create(const string& name, const string& conf_str) {
  AppConfig conf;
  google::protobuf::TextFormat::ParseFromString(conf_str, &conf);
  if (conf.has_linear_method()) {
    return LM::createApp(name, conf.linear_method());
  // } else if (conf.has_graph_partition()) {
  //   ptr = GP::GraphPartition::create(conf.graph_partition());
  // } else if (conf.has_factor_machine()) {
  //   ptr = FM::FactorizationMachine::create(conf.factorization_machine());
  // } else if (conf.has_cxxnet()) {
    // ptr = CXXNET::createApp(conf.app_name(), conf.cxxnet());
  } else {
    // ptr = new AppTest(conf.app_name());
  }
  return nullptr;
}
} // namespace PS

int main(int argc, char *argv[]) {
  PS::Postoffice::instance().start(argc, argv);
  // if (FLAGS_log_instant) FLAGS_logbuflevel = -1;

  PS::Postoffice::instance().stop();
  return 0;
}
