#pragma once

#include "neural_network/layer.h"
namespace PS {
namespace NN {

template<typename V>
class DataLayer : public Layer<V>  {
 public:
  void init() {

  }

  // forward the data
  V forward() {
    return 0;
  }

  // do nothing
  void backward() {}
};

} // namespace NN
} // namespace PS
