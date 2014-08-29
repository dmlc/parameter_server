#pragma once

#include "neural_network/layer.h"

namespace PS {
namespace NN {

template<typename V>
class SparseFullyConnectedLayer : public Layer<V>  {
 public:
  USING_LAYER;
  void init() {
    CHECK_EQ(in_layers_.size(), 1);
    CHECK_EQ(out_layers_.size(), 1);
    size_t my_size = this->size();
    size_t in_size = in_layers_[0]->size();
    // size_t nnz = my_size * in_size;
    if (cf_.init().type() == ParameterInitConfig::CLONE)
      return;

    model_ = ParameterPtr<V> (new Parameter<V>(this->name() + "_model"));
    model_->spa_value.set_empty_key(-1);
    fea_count_.set_empty_key(-1);

    // model_->gradient = MatrixPtr<V>(new DenseMatrix<V>(in_size, my_size));
    // model_->value = MatrixPtr<V>(new DenseMatrix<V>(in_size, my_size));
    // model_->value->value().setValue(cf_.init());
    // model_->value->eigenMatrix() = Matrix<V>::EMat::Random(in_size, my_size);
  }

  V forward() {
    CHECK(out_args_[0]->value);

    auto X = static_pointer_cast<SparseMatrix<Key, V>(in_args_[0]->value);
    CHECK(X->rowMajor());
    int p = size();
    size_t n = X->rows();
    out_args_[0]->value->resize(n, p);
    out_args_[0]->gradient->resize(n, p);
    auto Y = out_args_[0]->value->eigenMatrix();

    for (auto k : X->index()) ++ fea_count_[k];

    auto os = X->offset().data();
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = *os; j < *(os+1); ++os) {
        auto k = X->index()[j];
        auto m = model_->spa_value.find(k);
        if (m == model_->spa_value.end()) {
          if (fea_count_[k] < 50) continue;
          model_->spa_value[k].resize(p);
          m = model_->spa_value.find(k);
        }
        for (int l = 0; l < p; ++l) {
          if (m.second(l) != 0) Y(i,l) += m.second(l);
        }
      }
    }

    CHECK(activation_);
    activation_->forward(out_args_[0]);
    return 0;
  }

  void backward() {
    activation_->backward(out_args_[0]);

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
 protected:
  google::dense_hash_map<Key, int> fea_count_;
  // std::unordered_map<Key, int> fea_count_;

};

} // namespace NN
} // namespace PS
