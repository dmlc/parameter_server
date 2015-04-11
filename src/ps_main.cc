#include "ps.h"
namespace PS {

App* App::Create(const string& conf) {
  auto my_role = MyNode().role();
  if (my_role == Node::SERVER) {
    return CreateServerNode(conf);
  }
  return new App();
}
} // namespace PS

int main(int argc, char *argv[]) {
  auto& sys = PS::Postoffice::instance();
  sys.Run(&argc, &argv);

  int ret = 0;
  if (PS::MyNode().role() == PS::Node::WORKER) {
    ret = WorkerNodeMain(argc, argv);
  }

  sys.Stop();
  return ret;
}
