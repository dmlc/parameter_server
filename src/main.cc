#include "system/postoffice.h"
#include "data/show_example.h"
#include "data/text2proto.h"
DEFINE_bool(log_instant, false, "disable buffer of glog");

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
