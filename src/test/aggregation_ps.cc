#include "ps.h"
namespace PS {

DEFINE_int32(n, 100, "# of aggregation");
DEFINE_int32(interval, 100000, "time (usec) between two aggregation");

class Server : public App {
 public:
  virtual void Run() {
    WaitWorkersReady();
    for (int i = 0; i < FLAGS_n; ++i) {
      WaitReceivedRequest(i, kWorkerGroup);
      LL << MyNodeID() << " " << i;
    }
    LL << MyNodeID() << " done";
  }
};

class Worker : public App {
 public:
  virtual void Run() {
    for (int i = 0; i < FLAGS_n; ++i) {
      int ts = Submit(Task(), kServerGroup);
      usleep(FLAGS_interval);
      Wait(ts);
      LL << MyNodeID() << " " << i;
    }
    LL << MyNodeID() << " done";
  }
};

App* App::Create(const std::string& conf) {
  if (IsWorker()) return new Worker();
  if (IsServer()) return new Server();
  return new App();
}

}  // namespace PS

int main(int argc, char *argv[]) {
  return PS::RunSystem(argc, argv);
}
