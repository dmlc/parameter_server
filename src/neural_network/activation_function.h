#pragma once

#include "proto/neural_network.pb.h"
#include "neural_network/blob.h"

namespace PS {
namespace NN {

template<typename V> class ActivationFunction;
template<typename V>
using ActivationFuncPtr = std::shared_ptr<ActivationFunction<V>>;
template<typename V> class ReluActivation;

template <typename V>
class ActivationFunction {
 public:
  static ActivationFuncPtr<V> create(const ActivationConfig& config) {
    typedef ActivationConfig Type;
    switch (config.type()) {
      case Type::RELU:
        return ActivationFuncPtr<V>(new ReluActivation<V>());
      default:
        CHECK(false) << "unknown type: " << config.DebugString();
    }
    return ActivationFuncPtr<V>(nullptr);
  }

  virtual void forward(BlobPtr<V>& arg) = 0;
  virtual void backward(BlobPtr<V>& arg) = 0;
};


template <typename V>
class ReluActivation : public ActivationFunction<V> {
 public:
  void forward(BlobPtr<V>& arg) {
    CHECK(arg->value);
    auto X = arg->value->value();

    for (size_t i = 0; i < X.size(); ++i)
      X[i] = X[i] < 0 ? 0 : X[i];
  }

  void backward(BlobPtr<V>& arg) {
    CHECK(arg->value);
    CHECK(arg->gradient);
    auto X = arg->value->value();
    auto G = arg->gradient->value();
    CHECK_EQ(X.size(), G.size());

    for (size_t i = 0; i < X.size(); ++i)
      G[i] = X[i] < 0 ? 0 : X[i];
  }
};

} // namespace NN
} // namespace PS
