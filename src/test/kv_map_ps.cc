/**
 * @brief  Simple test of KVMap
 */
#include "ps.h"
#include "parameter/kv_map.h"
#include "parameter/kv_vector.h"
namespace PS {
typedef uint64 K;  // key
typedef float V;   // value type

struct Entry {
  void Get(V* data, void* state) { *data = value; }
  void Set(const V* data, void* state) { value += *data; }
  V value = 0;
};

class Server : public App {
 private:
  KVMap<K, V, Entry> vec_;
};

class Worker : public App {
 public:
  virtual void Run() {
    std::cout << MyNodeID() << ": this is worker " << MyRank() << std::endl;

    SArray<K> key;
    if (MyRank() == 0) {
      key = {0, 2, 4, 5};
    } else {
      key = {0, 1, 3, 4};
    }
    SArray<V> val = {1, 1, 1, 1};

    vec_.Wait(vec_.Push(Parameter::Request(0), key, {val}));
    vec_.Wait(vec_.Pull(Parameter::Request(0), key));

    std::cout << MyNodeID() << ": pulled value in channel 0 " << vec_[0].value
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
