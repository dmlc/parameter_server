#pragma once

#include "loss/loss.h"
#include "base/shared_array.h"
#include "base/matrix.h"
#include <Eigen/Dense>

namespace PS {

// Abstract base class for encapsulating a scalar loss object, that is, a loss
// which takes as input a real value prediction and a real valued label and
// outputs a non-negative loss value. Examples include the hinge hinge loss,
// binary classification loss, and univariate regression loss.
template <typename T>
class ScalarLoss : public Loss<T> {
 public:
  typedef Eigen::Map<Eigen::Array<T, Eigen::Dynamic, 1> > EArray;

  // evaluate the loss value
  virtual T evaluate(const EArray& y, const EArray& Xw) = 0;

  // compute gradients. Skip computing if grad (diag_hessian) is not
  // pre-allocated, namely grad.size() == 0 or diag_hessian.size() == 0
  virtual void compute(const EArray& y, const MatrixPtr<T>& X, const EArray& Xw,
                       EArray gradient, EArray diag_hessian) = 0;

  // implement virtual functions of class Loss
  // evaluate the loss value
  T evaluate(const MatrixPtrList<T>& data) {
    CHECK_EQ(data.size(), 2);
    SArray<T> y(data[0]->value());
    SArray<T> Xw(data[1]->value());
    CHECK_EQ(y.size(), Xw.size());

    return evaluate(y.eigenArray(), Xw.eigenArray());
  }

  // compute the gradients
  void compute(const MatrixPtrList<T>& data, MatrixPtrList<T> gradients) {
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

  // convenient wrapper using shared array
  // T value(const SArray<T>& y, const SArray<T>& Xw) {
  //   return evaluate(y.array(), Xw.array());
  // }

  // SArray<T> gradient(const SArray<T>& y, const SArray<T>& Xw, const MatrixPtr<T>& X) {
  //   SArray<T> grad(X->cols());
  //   compute(y.array(), Xw.array(), X, grad.array(), SArray<T>().array());
  //   return grad;
  // }

};

} // namespace PS
