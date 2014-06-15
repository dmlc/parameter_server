#pragma once

#include "loss/binary_classification_loss.h"

namespace PS {

template <typename T>
class LogitLoss : public BinaryClassificationLoss<T> {

 public:
  typedef Eigen::Map<Eigen::Array<T, Eigen::Dynamic, 1> > EArray;

  T evaluate(const EArray& y, const EArray& Xw) {
    return log( 1 + exp( -y * Xw )).sum();
  }

  void compute(const EArray& y, const MatrixPtr<T>& X, const EArray& Xw,
               EArray gradient, EArray diag_hessian) {
    auto tau = 1 / ( 1 + exp( y * Xw ));

    if (gradient.size() != 0)
      gradient = X->transTimes( -y * tau );

    if (diag_hessian.size() != 0)
      diag_hessian = X->dotTimes(X)->transTimes( tau * ( 1 - tau ));
  }
};

} // namespace PS
