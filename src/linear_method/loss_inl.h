#pragma once
#include "linear_method/loss.h"

namespace PS {
namespace LM {

template <typename T>
T ScalarLoss<T>::evaluate(const MatrixPtrList<T>& data) {
  CHECK_EQ(data.size(), 2);
  SArray<T> y(data[0]->value());
  SArray<T> Xw(data[1]->value());
  CHECK_EQ(y.size(), Xw.size());
  return evaluate(y.eigenArray(), Xw.eigenArray());
}


template <typename T>
void ScalarLoss<T>::compute(
    const MatrixPtrList<T>& data, MatrixPtrList<T> gradients) {
  if (gradients.size() == 0) return;

  CHECK_EQ(data.size(), 3);
  auto y = data[0]->value();
  auto X = data[1];
  auto Xw = data[2]->value();

  CHECK_EQ(y.size(), Xw.size());
  CHECK_EQ(y.size(), X->rows());

  CHECK(gradients[0]);
  auto gradient = gradients[0]->value();
  auto  diag_hessian =
      gradients.size()>1 && gradients[1] ? gradients[1]->value() : SArray<T>();
  if (gradient.size() != 0) CHECK_EQ(gradient.size(), X->cols());
  if (diag_hessian.size() != 0) CHECK_EQ(diag_hessian.size(), X->cols());

  compute(y.eigenArray(), X, Xw.eigenArray(), gradient.eigenArray(), diag_hessian.eigenArray());
}

} // namespace LM
} // namespace PS
