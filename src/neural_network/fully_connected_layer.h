#pragma once

#include "neural_network/layer.h"

namespace PS {
namespace NN {

template<typename V>
class FullyConnectedLayer : public Layer<V>  {
 public:
  void initModel() {
    CHECK_EQ(in_layers_.size(), 1);
    CHECK_EQ(out_layers_.size(), 1);
    size_t my_size = this->size();
    size_t in_size = in_layers_[0]->size();
    size_t nnz = my_size * in_size;
    model_->value->resize(in_size, my_size, nnz, true);
    model_->gradient->resize(in_size, my_size, nnz, true);
  }

  V forward() {
    CHECK(out_args_[0]->value);
    auto X = in_args_[0]->value->eigenMatrix();
    auto W = model_->value->eigenMatrix();
    auto Y = out_args_[0]->value->eigenMatrix();

    Y = X * W;

    this->activation_->forward(out_args_[0]);
    return 0;
  }

  void backward() {
    this->activation_->backward(out_args_[0]);

    auto X = in_args_[0]->value->eigenMatrix();
    auto Z = out_args_[0]->gradient->eigenMatrix();
    auto Wg = model_->gradient->eigenMatrix();

    Wg = X.transpose() * Z;

    if (in_args_[0]->gradient) {
      auto W = model_->value->eigenMatrix();
      auto Xg = in_args_[0]->gradient->eigenMatrix();

      Xg = Z * W.transpose();
    }
  }
 protected:
  using Layer<V>::in_args_;
  using Layer<V>::in_layers_;
  using Layer<V>::out_args_;
  using Layer<V>::out_layers_;
  using Layer<V>::model_;
};

} // namespace NN
} // namespace PS
