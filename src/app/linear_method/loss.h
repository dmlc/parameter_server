#pragma once
#include "util/common.h"
#include "util/matrix.h"
#include "linear_method/proto/lm.pb.h"
#include <Eigen/Dense>

namespace PS {
namespace LM {

template<typename T> class Loss {
 public:
  // evaluate the loss value
  virtual T evaluate(const MatrixPtrList<T>& data) = 0;
  // compute the gradients
  virtual void compute(const MatrixPtrList<T>& data, MatrixPtrList<T> gradients) = 0;
};


// scalar loss, that is, a loss which takes as input a real value prediction and
// a real valued label and outputs a non-negative loss value. Examples include
// the hinge hinge loss, binary classification loss, and univariate regression
// loss.
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

  T evaluate(const MatrixPtrList<T>& data) {
    CHECK_EQ(data.size(), 2);
    SArray<T> y(data[0]->value());
    SArray<T> Xw(data[1]->value());
    CHECK_EQ(y.size(), Xw.size());
    return evaluate(y.eigenArray(), Xw.eigenArray());
  }

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

};

// label = 1 or -1
template <typename T>
class BinaryClassificationLoss : public ScalarLoss<T> {
};


template <typename T>
class LogitLoss : public BinaryClassificationLoss<T> {

 public:
  typedef Eigen::Array<T, Eigen::Dynamic, 1> EArray;
  typedef Eigen::Map<EArray> EArrayMap;

  T evaluate(const EArrayMap& y, const EArrayMap& Xw) {
    return log( 1 + exp( -y * Xw )).sum();
  }

  void compute(const EArrayMap& y, const MatrixPtr<T>& X, const EArrayMap& Xw,
               EArrayMap gradient, EArrayMap diag_hessian) {
    // Do not use "auto tau = ...". It will return an expression and slow down
    // the performace.
    EArray tau = 1 / ( 1 + exp( y * Xw ));

    if (gradient.size())
      gradient = X->transTimes( -y * tau );

    if (diag_hessian.size())
      diag_hessian = X->dotTimes(X)->transTimes( tau * ( 1 - tau ));
  }
};

template <typename T>
class SquareHingeLoss : public BinaryClassificationLoss<T> {
 public:
  typedef Eigen::Array<T, Eigen::Dynamic, 1> EArray;
  typedef Eigen::Map<EArray> EArrayMap;

  T evaluate(const EArrayMap& y, const EArrayMap& Xw) {
    return (1- y * Xw).max(EArray::Zero(y.size())).square().sum();
  }

  void compute(const EArrayMap& y, const MatrixPtr<T>& X, const EArrayMap& Xw,
               EArrayMap gradient, EArrayMap diag_hessian) {
    gradient = - 2 *  X->transTimes(y * (y * Xw > 1.0).template cast<T>());
  }
};

template<typename T>
using LossPtr = std::shared_ptr<Loss<T>>;

template<typename T>
static LossPtr<T> createLoss(const LossConfig& config) {
  switch (config.type()) {
    case LossConfig::LOGIT:
      return LossPtr<T>(new LogitLoss<T>());
    case LossConfig::SQUARE_HINGE:
      return LossPtr<T>(new SquareHingeLoss<T>());
    default:
      CHECK(false) << "unknown type: " << config.DebugString();
  }
  return LossPtr<T>(nullptr);
}

} // namespace LM
} // namespace PS
