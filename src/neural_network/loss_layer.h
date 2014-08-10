#pragma once

#include "neural_network/layer.h"
namespace PS {
namespace NN {

template<typename V>
class LossLayer : public Layer<V>  {
 public:
  void init() { }
 protected:
  USING_LAYER;
  void checkValue(int n) {
    CHECK_GE(in_args_.size(), n);
    for (int i = 0; i < n; ++i) {
      CHECK(in_args_[i]->value);
      CHECK_EQ(in_args_[i]->value->rows(), in_args_[0]->value->rows());
      CHECK_EQ(in_args_[i]->value->cols(), in_args_[0]->value->cols());
    }
  }

  void checkGradient(int n) {
    CHECK_GE(in_args_.size(), n);
    for (int i = 0; i < n; ++i) {
      CHECK(in_args_[i]->gradient);
      CHECK_EQ(in_args_[i]->gradient->rows(), in_args_[0]->gradient->rows());
      CHECK_EQ(in_args_[i]->gradient->cols(), in_args_[0]->gradient->cols());
    }
  }
};

// ln (1+exp(-y*x))
template<typename V>
class LogisticLossLayer : public LossLayer<V> {
 public:
  USING_LAYER;
  using LossLayer<V>::checkValue;
  using LossLayer<V>::checkGradient;
  V forward() {
    checkValue(2);
    auto X = in_args_[0]->value->eigenArray();
    auto Y = in_args_[1]->value->eigenArray();

    return (log( 1 + exp( -X * Y )).sum());
  }

  void backward() {
    checkValue(2);
    checkGradient(1);
    auto X = in_args_[0]->value->eigenArray();
    auto Y = in_args_[1]->value->eigenArray();
    auto Xg = in_args_[0]->gradient->eigenArray();

    Xg = - Y / ( 1 + exp( Y * X ));

    // LL << in_args_[0]->name;
    // LL << in_args_[0]->gradient->debugString();
  }
};


} // namespace NN
} // namespace PS
