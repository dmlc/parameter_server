/**
 * @brief  Simple test of KVLayer
 */
#include "ps.h"
#include "parameter/kv_layer.h"
namespace PS {
typedef uint64 K;  // key type
typedef int V;     // value type

class Updater {
 public:
  void Init(int id, size_t size, V* data) {
    memset(data, 0, sizeof(V)*size);
  }

  void Update(int id, size_t size, const V* recv_data, V* data) {
    // sum
    for (int i = 0; i < size; ++i) {
      data[i] += recv_data[i];
    }
  }
};

class Server : public App {
 public:
  Server() {
    model_.set_updater(&updt_);
  }
 private:
  KVLayer<V, Updater> model_;
  Updater updt_;
};

class Worker : public App {
 public:
  virtual void Run() {
    std::cout << MyNodeID() << ": this is worker " << MyRank() << std::endl;

    std::vector<size_t> layer_size = {5, 10, 4};
    int n = layer_size.size();

    std::vector<SArray<V>> layers(n);
    for (int i = 0; i < n; ++i) layers[i].resize(layer_size[i]);

    std::vector<int> pull_time(n);
    for (int i = 0; i < n; ++i) {
      auto& val = layers[i];
      val.SetValue(1);
      int ts = model_.Push(
          Parameter::Request(i), val.data(), val.size());
      pull_time[i] = model_.Pull(
          Parameter::Request(i, -1, {ts}), val.data(), val.size(),
          [i, this](){
            LL << "layer " << i << " " << model_[i];
          });
    }

    for (int i = 0; i < n; ++i) {
      model_.Wait(pull_time[i]);
    }
    sleep(1);
  }
 private:
  KVLayer<V> model_;
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
