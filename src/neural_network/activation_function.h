#pragma once

#include "proto/neural_network.pb.h"
#include "neural_network/parameter.h"

namespace PS {
namespace NN {

template<typename V> class ActivationFunction;
template<typename V>
using ActivationFuncPtr = std::shared_ptr<ActivationFunction<V>>;
template<typename V> class ReluActivation;
template<typename V> class IdentityActivation;

template <typename V>
class ActivationFunction {
 public:
  static ActivationFuncPtr<V> create(const ActivationConfig& config) {
    typedef ActivationConfig Type;
    switch (config.type()) {
      case Type::RELU:
        return ActivationFuncPtr<V>(new ReluActivation<V>());
      case Type::IDENTITY:
        return ActivationFuncPtr<V>(new IdentityActivation<V>());
      default:
        CHECK(false) << "unknown type: " << config.DebugString();
    }
    return ActivationFuncPtr<V>(nullptr);
  }

  virtual void forward(ParameterPtr<V>& arg) = 0;
  virtual void backward(ParameterPtr<V>& arg) = 0;
};


template <typename V>
class ReluActivation : public ActivationFunction<V> {
 public:
  void forward(ParameterPtr<V>& arg) {
    CHECK(arg->value);
    auto X = arg->value->value();

    for (size_t i = 0; i < X.size(); ++i)
      X[i] = X[i] < 0 ? 0 : X[i];
  }

  void backward(ParameterPtr<V>& arg) {
    CHECK(arg->gradient);
    auto G = arg->gradient->value();

    for (size_t i = 0; i < G.size(); ++i)
      G[i] = G[i] < 0 ? 0 : G[i];
  }
};

template <typename V>
class IdentityActivation : public ActivationFunction<V> {
 public:
  void forward(ParameterPtr<V>& arg) {
  }

  void backward(ParameterPtr<V>& arg) {
    // CHECK(arg->value);
    // CHECK(arg->gradient);
    // auto X = arg->value->value();
    // auto G = arg->gradient->value();
    // CHECK_EQ(X.size(), G.size());

    // G.copyFrom(X);
  }
};

} // namespace NN
} // namespace PS
