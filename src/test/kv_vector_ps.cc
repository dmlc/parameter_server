#include "ps.h"
#include "parameter/kv_vector.h"
namespace PS {
typedef uint64 K;  // key
typedef int V1;    // value type 1
typedef float V2;  // value type 2

class Server : public App {
 public:
  Server() : vec1_(), vec2_(true, 2) {
    SArray<K> key = {0, 1, 3, 4, 5};
    // channel 0
    vec1_[0].key = key;
    vec1_[0].value = {1, 2, 3, 4, 5};

    // channel 1
    vec1_[1].key = key;
    vec1_[1].value = {2, 3, 4, 5, 6};

    // channel 4
    vec2_[4].key = key;
  }

  virtual void Run() {

  }
 private:
  KVVector<K, V1> vec1_;
  KVVector<K, V2> vec2_;
};

class Worker : public App {
 public:
  Worker() : vec1_(), vec2_(true, 2) { }

  virtual void Run() {
    WaitServersReady();
    std::cout << MyNodeID() << ": this is worker " << MyRank() << std::endl;

    SArray<K> key;
    if (MyRank() == 0) {
      key = {0, 2, 4, 5};
    } else {
      key = {0, 1, 3, 4};
    }

    int ts1 = vec1_.Pull(Parameter::Request(0), key);
    int ts2 = vec1_.Pull(Parameter::Request(1), key);

    vec1_.Wait(ts1);
    std::cout << MyNodeID() << ": pulled value in channel 0 " << vec1_[0].value
              << std::endl;

    vec1_.Wait(ts2);
    std::cout << MyNodeID() << ": pulled value in channel 1 " << vec1_[1].value
              << std::endl;

    // push [1 1 1 1  and [3 3 3 3  into servers
    //       2 2 2 2]      4 4 4 4]
    SArray<V2> val1 = {1, 2, 1, 2, 1, 2, 1, 2};
    SArray<V2> val2 = {3, 4, 3, 4, 3, 4, 3, 4};

    // int ts3 = vec2_.Push(Parameter::Request(4), key, {val1, val2});
    // int ts4 = vec2_.Pull(Parameter::Request(4, {ts3+1}), key);
    // vec2_.Wait(ts4);

    // std::cout << MyNodeID() << ": pulled value in channel 4 " << vec2_[4].value;
  }
 private:
  KVVector<K, V1> vec1_;
  KVVector<K, V2> vec2_;
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
