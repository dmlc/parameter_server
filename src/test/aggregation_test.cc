#include "ps.h"
namespace PS {

DEFINE_int32(n, 100, "# of aggregation");
DEFINE_int32(interval, 100000, "time (usec) between two aggregation");

class AggregationServer : public App {
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

class AggregationWorker : public App {
 public:
  virtual void Run() {
    WaitServersReady();
    for (int i = 0; i < FLAGS_n; ++i) {
      int ts = Submit(Task(), kServerGroup);
      usleep(FLAGS_interval);
      Wait(ts, kServerGroup);
      LL << MyNodeID() << " " << i;
    }
    LL << MyNodeID() << " done";
  }
};

App* App::Create(const std::string& conf) {
  if (IsWorker()) return new AggregationWorker();
  if (IsServer()) return new AggregationServer();
  return new App();
}

}  // namespace PS

int main(int argc, char *argv[]) {
  return PS::RunSystem(argc, argv);
}
