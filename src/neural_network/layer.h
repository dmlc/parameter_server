#pragma once

#include "util/common.h"
#include "proto/neural_network.pb.h"
#include "base/matrix.h"
#include "neural_network/parameter.h"
#include "neural_network/activation_function_inl.h"

namespace PS {
namespace NN {

template<typename V> class Net;
template<typename V> class Layer;
template<typename V> using LayerPtr = std::shared_ptr<Layer<V>>;
template<typename V> using LayerPtrList = std::vector<LayerPtr<V>>;

template<typename V> class Layer {
 public:
  friend class Net<V>;
  void set(const LayerConfig& config) { cf_ = config; }

  // all layers in *in_layers_* have been inited
  virtual void init() { }
  virtual V forward() = 0;
  virtual void backward() = 0;

  ParameterPtr<V>& model() { return model_; }
  const LayerConfig& config() { return cf_; }

 protected:
  LayerConfig cf_;
  LayerPtrList<V> in_layers_, out_layers_;
  ParameterPtrList<V> in_args_, out_args_;
  ActivationFuncPtr<V> activation_;
  ParameterPtr<V> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Layer);
};

} // namespace NN
} // namespace PS
