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

  virtual void Slice(const Message& request, const std::vector<Range<Key>>& krs,
                     std::vector<Message*>* msgs) {
    for (auto m : *msgs) *m = request;
  }

  virtual void Run() {
    int n = FLAGS_n;
    int m = FLAGS_data_size;
    auto tv = tic();
    for (int j = 0; j < n; ++j) {
      SArray<int> val(m*1000/sizeof(int), 1);
      Message msg;
      msg.add_value(val);
      msg.recver = kServerGroup;
      int ts = Submit(&msg);
      Wait(ts);
    }
    double thr = (double)m / 1000.0 * n * sys_.manager().num_servers() / toc(tv);
    printf("%s: packet size: %d KB, throughput %.3lf MB/sec\n",
           MyNodeID().c_str(), m, thr);
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
