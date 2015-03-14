#include "ps.h"
#include "util/resource_usage.h"

DEFINE_int32(n, 1000, "repeat n times");
DEFINE_int32(data_size, 1000,
              "data in KB sent from a worker to a server");
DEFINE_bool(server_aggregation, false,
            "servers will aggregate the data from servs if true");
namespace PS {

class Server : public App {
 public:
  virtual void Run() {
    // if (FLAGS_server_aggregation) {
    //   WaitWorkersReady();
    //   auto data = split(FLAGS_data_size, ',',true);
    //   for (int i = 0; i < data.size() * FLAGS_n; ++i) {

    //   }
    // }
  }
};

class Worker : public App {
 public:
  virtual void Run() {
    WaitServersReady();
    int n = FLAGS_n;
    int m = FLAGS_data_size;
    SArray<double> time(n);
    for (int j = 0; j < n; ++j) {
      SArray<int> val(m*1000/sizeof(int), 1);
      auto msg = NewMessage();
      msg->add_value(val);
      msg->recver = kServerGroup;
      auto tv = tic();
      int ts = Submit(msg);
      Wait(ts);
      time[j] = toc(tv);
    }
    double thr = (double)m / 1000.0 * n * sys_.manager().numServers() / time.sum();
    printf("%s: packet size: %d KB, latency: %lf +- %lf sec, throughput %.3lf MB/sec\n",
           MyNodeID().c_str(), m, time.mean(), time.std(), thr);
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
