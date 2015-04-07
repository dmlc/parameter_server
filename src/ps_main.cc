#include "ps.h"
namespace ps {

App* App::Create(const string& conf) {
  auto my_role = MyNode().role();
  if (my_role == Node::SERVER) {
    return CreateServerNode(conf);
  }
  return new App();
}
} // namespace ps

int main(int argc, char *argv[]) {
  auto& sys = ps::Postoffice::instance();
  sys.Run(&argc, &argv);

  int ret = 0;
  if (ps::MyNode().role() == ps::Node::WORKER) {
    ret = WorkerNodeMain(argc, argv);
  }

  sys.Stop();
  return ret;
}
