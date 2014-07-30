#pragma once

#include "proto/neural_network.pb.h"
#include "neural_network/layer.h"

namespace PS {
namespace NN {

template<typename V> class Net {
 public:
  // construct the network
  void init(const NetConfig& config) {
    cf_ = config;
  }

  LayerPtrList<V>& layers() { return layers_; }

 protected:
  LayerPtrList<V> layers_;
  NetConfig cf_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Net);
};

} // namespace NN
} // namespace PS
