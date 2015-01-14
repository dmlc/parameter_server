#pragma once

#include "util/common.h"
// #include "proto/neural_network.pb.h"
#include "util/matrix.h"
#include "neural_network/parameter.h"
#include "neural_network/activation_function_inl.h"

namespace PS {
namespace NN {

template<typename V> class Layer;
template<typename V> using LayerPtr = std::shared_ptr<Layer<V>>;
template<typename V> using LayerPtrList = std::vector<LayerPtr<V>>;

#define USING_LAYER                             \
  using Layer<V>::in_args_;                     \
  using Layer<V>::in_layers_;                   \
  using Layer<V>::out_args_;                    \
  using Layer<V>::out_layers_;                  \
  using Layer<V>::model_;                       \
  using Layer<V>::activation_;                  \
  using Layer<V>::size;                         \
  using Layer<V>::cf_;

template<typename V> class Layer {
 public:
  Layer() { }
  void set(const LayerConfig& config) {
    cf_ = config;
    if (cf_.has_activation())
      activation_ = ActivationFunction<V>::create(cf_.activation());
  }

  size_t size() { return cf_.size(); }

  // all in layers and out layers have been added
  virtual void init() = 0;
  virtual V forward() = 0;
  virtual void backward() = 0;

  void addInLayer(const LayerPtr<V>& layer, const string& edge) {
    CHECK_EQ(name(), layer->out_layers_.back()->name());
    in_args_.push_back(layer->out_args_.back());
    in_layers_.push_back(layer);
  }

  void addOutLayer(const LayerPtr<V>& layer, const string& edge) {
    out_layers_.push_back(layer);
    ParameterPtr<V> arg(new Parameter<V>(edge));
    // arg->value = MatrixPtr<V>(new DenseMatrix<V>());
    if (!(cf_.type() == LayerConfig::DATA)) {
      // arg->gradient = MatrixPtr<V>(new DenseMatrix<V>());
    }
    out_args_.push_back(arg);
  }

  string name() { return cf_.name(); }
  ParameterPtr<V>& model() { return model_; }
  const LayerConfig& config() { return cf_; }

  ParameterPtrList<V>& inArgs() { return in_args_; }
  ParameterPtrList<V>& outArgs() { return out_args_; }
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
