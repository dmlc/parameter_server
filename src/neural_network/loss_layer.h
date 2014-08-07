#pragma once

#include "neural_network/layer.h"
namespace PS {
namespace NN {

template<typename V>
class LossLayer : public Layer<V>  {
 public:
  void init() { }
 protected:
  void checkValue(int n) {
    CHECK_EQ(in_args_.size(), n);
    for (int i = 0; i < n; ++i) {
      CHECK(in_args_[i]->value);
      CHECK_EQ(in_args_[i]->value->rows(), in_args_[0]->value->rows());
      CHECK_EQ(in_args_[i]->value->cols(), in_args_[0]->value->cols());
    }
  }

  //   CHECK(in_args_[0]->gradient);
  //   CHECK_EQ(in_args_[0]->gradient->rows(), in_args_[0]->value->rows());
  //   CHECK_EQ(in_args_[0]->gradient->cols(), in_args_[0]->value->cols());
  // }
  using Layer<V>::in_args_;
  using Layer<V>::in_layers_;
  using Layer<V>::out_args_;
  using Layer<V>::out_layers_;
  using Layer<V>::model_;
};

// ln (1+exp(-y*x))
template<typename V>
class LogisticLossLayer : public LossLayer<V> {
 public:
  V forward() {
    // this->checkSalarLoss();
    auto X = this->in_args_[0]->value->eigenArray();
    auto Y = this->in_args_[1]->value->eigenArray();

    return (log( 1 + exp( -X * Y )).sum());
  }

  void backward() {
    // this->checkSalarLoss();
    auto X = this->in_args_[0]->value->eigenArray();
    auto Y = this->in_args_[1]->value->eigenArray();
    auto Xg = this->in_args_[0]->gradient->eigenArray();

    Xg = - Y / ( 1 + exp( Y * X ));
  }
};


} // namespace NN
} // namespace PS
