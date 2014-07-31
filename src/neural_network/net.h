#pragma once

#include "proto/neural_network.pb.h"
#include "neural_network/layer_inl.h"

namespace PS {
namespace NN {

template<typename V> class Net {
 public:
  Net(const NetConfig& config) : cf_(config) { }

  // construct the network
  void init();

  LayerPtrList<V>& layers() { return layers_; }

 protected:
  LayerPtrList<V> layers_;
  NetConfig cf_;

  std::map<string, int> layer_ids_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Net);
};

template<typename V>
Net<V>::init() {
  CHECK_GT(cf_.layer_size(), 0);
  layers_.clear();
  for (int id = 0; id < cf_.layer_size(); ++id) {
    auto f = cf_.layer(id);
    auto l = LayerFactory<V>::create(f);
    for (int i = 0; i < f.in_size(); ++i) {
      auto in = f.in(i);
      CHECK_EQ(layer_ids_.count(in), 1)
          << "layers should be in a bottom-to-up order for the config file";
      int in_id = layer_ids_[in];
      l->in_layers_.push_back(layers_[in_id]);
      layers_[in_id].out_layers_.push_back(l);
    }
    l->init();
    layers_.push_back(l);
    layer_ids_[f.name()] = id;
  }
}

} // namespace NN
} // namespace PS
