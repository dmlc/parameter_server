#pragma once
#include "proto/neural_network.pb.h"

namespace PS {
namespace NN {

template<typename V> class ActivationFunction;
template<typename V>
using ActivationFuncPtr = std::shared_ptr<ActivationFunction<V>>;

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

  virtual void forward(Argument<V>& aug) = 0;
  virtual void backward(Argument<V>& aug) = 0;
};


template <typename V>
class ReluActivation : public ActivationFunction<V> {
 public:
  void forward(Argument<V>& aug);
  void backward(Argument<V>& aug);
}

} // namespace NN
} // namespace PS
