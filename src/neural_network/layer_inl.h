#pragma once

#include "neural_network/layer.h"
#include "neural_network/loss_layer.h"
#include "neural_network/fully_connected_layer.h"

namespace PS {
namespace NN {

template<typename V> class LayerFactory {
 public:
  static LayerPtr<V> create(const LayerConfig& config) {
    LayerPtr<V> ptr;
    switch (config.type()) {
      case LayerConfig::FULLY_CONNECTED:
        ptr = LayerPtr<V>(new FullyConnectedLayer<V>());
        break;
      case LayerConfig::LOGISTIC_LOSS:
        ptr = LayerPtr<V>(new LogisticLossLayer<V>());
        break;
      default:
        CHECK(false) << "unknown type: " << config.DebugString();
    }
    ptr->set(config);
    return ptr;
  }
};

} // namespace NN
} // namespace PS
