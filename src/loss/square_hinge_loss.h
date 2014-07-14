#pragma once

#include "loss/binary_classification_loss.h"

namespace PS {

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

    // LL << gradient.matrix().squaredNorm();
  }
};

} // namespace PS
