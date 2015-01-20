#include "ps.h"
#include "system/app.h"
namespace PS {

App* App::create(const string& name, const string& conf) {
  auto my_role = Postoffice::instance().myNode().role();
  if (my_role == Node::SERVER) {
    return CreateServer(conf);
  } else {
    return new App();
  }
}
} // namespace PS

int main(int argc, char *argv[]) {
  auto& sys = PS::Postoffice::instance();
  sys.start(argc, argv);

  int ret = 0;
  if (sys.myNode().role() == PS::Node::WORKER) {
    ret = PSMain(argc, argv);
    // using namespace PS;
    // LL << ret;
  }

  sys.stop();
  return ret;
}
