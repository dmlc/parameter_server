#pragma once

#include "util/common.h"
#include "proto/neural_network.pb.h"
#include "base/matrix.h"

namespace PS {
namespace NN {

template<typename V> class Layer {
 public:
  explicit Layer(const LayerConfig& config)
      : cf_(config) { }

  virtual void init() { }

  virtual V forward(const MatrixPtr<V>& bottom, MatrixPtr<V> top) = 0;
  virtual V backward(const MatrixPtr<V>& top, MatrixPtr<V> bottom) = 0;

 protected:
  LayerConfig cf_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Layer);
};

} // namespace NN
} // namespace PS
