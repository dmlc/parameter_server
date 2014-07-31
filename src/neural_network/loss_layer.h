#pragma once

namespace PS {
namespace NN {

template<typename V>
class LossLayer : public Layer<V>  {
 protected:
  void checkSalarLoss() {
    CHECK_EQ(in_args_.size(), 2);
    CHECK(in_args_[0]->value);
    CHECK(in_args_[1]->value);
    CHECK_EQ(in_args_[0]->value->rows(), in_args_[1]->value->rows());
    CHECK_EQ(in_args_[0]->value->cols(), in_args_[1]->value->cols());
    CHECK(in_args_[0]->gradient);
    CHECK_EQ(in_args_[0]->gradient->rows(), in_args_[0]->value->rows());
    CHECK_EQ(in_args_[0]->gradient->cols(), in_args_[0]->value->cols());
  }
};

// ln (1+exp(-y*x))
template<typename V>
class LogisticLossLayer : public LossLayer<V> {
 public:
  V forward() {
    checkSalarLoss();
    auto X = in_args_[0]->value->eigenArray();
    auto Y = in_args_[1]->value->eigenArray();

    return (log( 1 + exp( -X * Y )).sum());
  }

  void backward() {
    checkSalarLoss();
    auto X = in_args_[0]->value->eigenArray();
    auto Y = in_args_[1]->value->eigenArray();
    auto Xg = in_args_[0]->gradient->eigenArray();

    Xg = - Y / ( 1 + exp( Y * X ));
  }
};


} // namespace NN
} // namespace PS
