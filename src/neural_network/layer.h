#pragma once

#include "util/common.h"
#include "proto/neural_network.pb.h"
#include "base/matrix.h"
#include "neural_network/argument.h"
namespace PS {
namespace NN {

template<typename V> class Layer;
template<typename V> using LayerPtr = std::shared_ptr<Layer<V>>;
template<typename V> using LayerPtrList = std::vector<LayerPtr<V>>;

template<typename V> class Layer {
 public:
  explicit Layer(const LayerConfig& config)
      : cf_(config) { }

  virtual void init() { }
  virtual void forward() = 0;
  virtual void backward() = 0;
  virtual void update() = 0;
 protected:
  LayerConfig cf_;
  LayerPtrList<V> in_layers_, out_layers_;
  ArgumentPtrList<V> in_args_, out_args_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Layer);
};

} // namespace NN
} // namespace PS
