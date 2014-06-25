#pragma once

#include "loss/binary_classification_loss.h"

namespace PS {

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

    if (gradient.size() != 0)
      gradient = X->transTimes( -y * tau );

    if (diag_hessian.size() != 0)
      diag_hessian = X->dotTimes(X)->transTimes( tau * ( 1 - tau ));
  }
};

} // namespace PS
