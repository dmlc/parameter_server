#pragma once

#include "neural_network/layer.h"

namespace PS {
namespace NN {

template<typename V>
class FullyConnectedLayer : public Layer<V>  {
 public:
  void init() {
    // TODO init model, output
    // weight_ = MatrixPtr<V>(new DenseMatrix<V>(cf_.in_size(), cf_.out_size()));
    out_args.clear();
    CHECK_EQ(out_layers)
  }

  V forward() {
    CHECK(out_args_[0]->value);
    auto X = in_args_[0]->value->eigenMatrix();
    auto W = model_->value->eigenMatrix();
    auto Y = out_args_[0]->value->eigenMatrix();

    Y = X * W;

    activation_->forward(out_args_[0]);
    return 0;
  }

  void backward() {
    activation_->backward(out_args_[0]);

    auto X = in_args_[0]->value->eigenMatrix();
    auto Z = out_args_[0]->gradient->eigenMatrix();
    auto Wg = model_->gradient->eigenMatrix();

    Wg = X.transpose() * Z;

    if (in_args_[0]->gradient) {
      auto W = model_->value->eigenMatrix()
      auto Xg = in_args_[0]->gradient->eigenMatrix();

      Xg = Z * W.tranpose();
    }
  }
};

} // namespace NN
} // namespace PS
