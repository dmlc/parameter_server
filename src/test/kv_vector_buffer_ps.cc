/**
 * @brief  Simple test of buffered KVVector
 */
#include "ps.h"
#include "parameter/kv_vector.h"
namespace PS {
typedef uint64 K;  // key
typedef float V;   // value type

class Server : public App {
 public:
  Server() : vec_(true, 2) {
    // channel 4
    vec_[4].key = {0, 1, 3, 4, 5};
  }

  virtual void Run() {
    // aggregate data received from all workers
    WaitWorkersReady();
    int ts = 0;
    vec_.WaitReceivedRequest(ts, kWorkerGroup);
    auto recv = vec_.buffer(ts);
    vec_[4].value = recv.values[0];
    vec_[4].value.vec() += recv.values[1].vec();
    vec_.FinishReceivedRequest(ts+1, kWorkerGroup);
  }
 private:
  KVVector<K, V> vec_;
};

class Worker : public App {
 public:
  Worker() : vec_(false, 2) { }

  virtual void Run() {
    std::cout << MyNodeID() << ": this is worker " << MyRank() << std::endl;

    SArray<K> key;
    if (MyRank() == 0) {
      key = {0, 2, 4, 5};
    } else {
      key = {0, 1, 3, 4};
    }
    // push [1 1 1 1  and [3 3 3 3  into servers
    //       2 2 2 2]      4 4 4 4]
    SArray<V> val1 = {1, 2, 1, 2, 1, 2, 1, 2};
    SArray<V> val2 = {3, 4, 3, 4, 3, 4, 3, 4};

    int ts = vec_.Push(Parameter::Request(4), key, {val1, val2});
    // this pull request will depends on a virtual timestamp ts+1 which is used for
    // aggregation
    vec_.Wait(vec_.Pull(Parameter::Request(4, ts+2, {ts+1}), key));

    std::cout << MyNodeID() << ": pulled value in channel 4 " << vec_[4].value
              << std::endl;
  }
 private:
  KVVector<K, V> vec_;
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
