#pragma once

#include "neural_network/layer.h"
#include "neural_network/loss_layer.h"
#include "neural_network/auc_layer.h"
#include "neural_network/data_layer.h"
#include "neural_network/fully_connected_layer.h"

namespace PS {
namespace NN {

template<typename V> class LayerFactory {
 public:
  static LayerPtr<V> create(const LayerConfig& config) {
    LayerPtr<V> ptr;
    typedef LayerConfig Type;
    switch (config.type()) {
      case Type::FULLY_CONNECTED:
        ptr = LayerPtr<V>(new FullyConnectedLayer<V>());
        break;
      case Type::LOGISTIC_LOSS:
        ptr = LayerPtr<V>(new LogisticLossLayer<V>());
        break;
      case Type::DATA:
        ptr = LayerPtr<V>(new DataLayer<V>());
        break;
      case Type::AUC:
        ptr = LayerPtr<V>(new AUCLayer<V>());
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
