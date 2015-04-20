/**
 * @file   kv_vector_ps.cc
 * @author Mu Li <muli@cs.cmu.edu>
 * @date   Wed Mar 18 16:34:09 2015
 *
 * @brief  Simple test of KVVector
 *
 *
 */

#include "ps.h"
#include "parameter/kv_vector.h"
namespace PS {
typedef uint64 K;  // key type
typedef int V;     // value type

class Server : public App {
 public:
  Server() : vec_() {
    // channel 0
    vec_[0].key   = {0, 1, 3, 4, 5};
    vec_[0].value = {1, 2, 3, 4, 5};

    // channel 1
    vec_[1].key   = {0, 1, 3, 4, 5};
    vec_[1].value = {2, 3, 4, 5, 6};
  }

 private:
  KVVector<K, V> vec_;
};

class Worker : public App {
 public:
  Worker() : vec_() { }

  virtual void Run() {
    std::cout << MyNodeID() << ": this is worker " << MyRank() << std::endl;

    SArray<K> key;
    if (MyRank() == 0) {
      key = {0, 2, 4, 5};
    } else {
      key = {0, 1, 3, 4};
    }

    int ts1 = vec_.Pull(Parameter::Request(0), key);
    int ts2 = vec_.Pull(Parameter::Request(1), key);

    vec_.Wait(ts1);
    std::cout << MyNodeID() << ": pulled value in channel 0 " << vec_[0].value
              << std::endl;

    vec_.Wait(ts2);
    std::cout << MyNodeID() << ": pulled value in channel 1 " << vec_[1].value
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
