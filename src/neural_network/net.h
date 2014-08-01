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

  // std::map<string, LayerPtr<V>> in_edges_;
  std::map<string, LayerPtr<V>> out_edges_;

  // std::map<string, int> layer_ids_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Net);
};

template<typename V>
void Net<V>::init() {
  CHECK_GT(cf_.layer_size(), 0);
  for (int id = 0; id < cf_.layer_size(); ++id) {
    auto f = cf_.layer(id);
    auto l = LayerFactory<V>::create(f);
    for (int i = 0; i < f.in_size(); ++i) {
      auto edge = f.in(i);
      CHECK_EQ(out_edges_.count(edge), 1)
          << "layers should be in a bottom-to-up order for the config file";
      auto in_layer = out_edges_[edge];
      // addOutLayer should be called before addInLayer
      in_layer->addOutLayer(l, edge);
      l->addInLayer(in_layer, edge);
    }
    layers_.push_back(l);
    for (int i = 0; i < f.out_size(); ++i) {
      auto edge = f.out(i);
      CHECK_EQ(out_edges_.count(edge), 0);
      out_edges_[edge] = l;
    }
  }
  for (auto& l : layers_) l->init();
}

} // namespace NN
} // namespace PS
