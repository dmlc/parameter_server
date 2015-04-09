#include "ps.h"

namespace ps {
App* App::Create(const string& conf) {
  return new App();
}
}  // namespace ps

int main(int argc, char *argv[]) {
  ps::StartSystem(argc, argv);

  int ret = 0;
  if (ps::IsWorkerNode()) {
    usleep(1000);
    ret = WorkerNodeMain(argc, argv);
  } else if (ps::IsServerNode()) {
    ret = CreateServerNode(argc, argv);
  }
  ps::StopSystem();
  return ret;
}
