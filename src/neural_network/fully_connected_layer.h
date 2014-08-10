#pragma once

#include "neural_network/layer.h"

namespace PS {
namespace NN {

template<typename V>
class FullyConnectedLayer : public Layer<V>  {
 public:
  USING_LAYER;
  void init() {
    CHECK_EQ(in_layers_.size(), 1);
    CHECK_EQ(out_layers_.size(), 1);
    size_t my_size = this->size();
    size_t in_size = in_layers_[0]->size();
    // size_t nnz = my_size * in_size;
    model_ = ParameterPtr<V> (new Parameter<V>(this->name() + "_model"));
    model_->gradient = MatrixPtr<V>(new DenseMatrix<V>(in_size, my_size));
    model_->value = MatrixPtr<V>(new DenseMatrix<V>(in_size, my_size));
    model_->value->value().setValue(cf_.init());
    // model_->value->eigenMatrix() = Matrix<V>::EMat::Random(in_size, my_size);
  }

  V forward() {
    CHECK(out_args_[0]->value);
    auto X = in_args_[0]->value->eigenMatrix();
    auto W = model_->value->eigenMatrix();
    out_args_[0]->value->resize(X.rows(), this->size());
    out_args_[0]->gradient->resize(X.rows(), this->size());
    auto Y = out_args_[0]->value->eigenMatrix();

    // LL << X.rows() << " " << X.cols();
    // LL << W.rows() << " " << W.cols();
    // LL << Y.rows() << " " << Y.cols();
    Y = X * W;

    CHECK(activation_);
    activation_->forward(out_args_[0]);
    return 0;
  }

  void backward() {
    this->activation_->backward(out_args_[0]);

    auto X = in_args_[0]->value->eigenMatrix();
    auto Z = out_args_[0]->gradient->eigenMatrix();
    auto Wg = model_->gradient->eigenMatrix();

    // LL << in_args_[0]->value->debugString();
    // LL << out_args_[0]->name;
    // LL << out_args_[0]->gradient->debugString();

    Wg = X.transpose() * Z;

    // LL << Wg(0,0);
    if (in_args_[0]->gradient) {
      auto W = model_->value->eigenMatrix();
      auto Xg = in_args_[0]->gradient->eigenMatrix();

      Xg = Z * W.transpose();
    }
  }
};

} // namespace NN
} // namespace PS
