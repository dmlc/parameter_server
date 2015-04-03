/**
 * @brief  Performance test of KVMap
 */
#include <random>
#include "ps.h"
#include "parameter/kv_map.h"
#include "parameter/kv_vector.h"
#include "util/shared_array_inl.h"
#include "util/resource_usage.h"
namespace PS {
DEFINE_int32(n, 10, "repeat n times");

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
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 21);
    size_t bytes = 0;
    auto tv = tic();
    for (int i = 0; i < FLAGS_n; ++i) {

      int k = dis(gen);
      SArray<K> key;
      key.ReadFromFile("../test/keys/key_" + std::to_string(k));
      SArray<V> val(key.size());
      ParamInitConfig cf;
      cf.set_type(ParamInitConfig::GAUSSIAN);
      cf.set_mean(0);
      cf.set_std(1);
      val.SetValue(cf);

      int ts = vec_.Push(Parameter::Request(i), key, {val});
      vec_.Wait(vec_.Pull(Parameter::Request(i, ts+1, {ts}), key));
      CHECK_EQ(vec_[i].value.size(), key.size());
      vec_.Clear(i);
      bytes += key.size() * (sizeof(K) + sizeof(V)) * 2;
    }

    double thr = (double) bytes / toc(tv) / 1000000;
    printf("%s: %d push/pull, throughput %.3lf MB/sec\n",
           MyNodeID().c_str(), FLAGS_n, thr);
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
