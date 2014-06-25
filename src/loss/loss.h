#pragma once
#include "util/common.h"
#include "base/matrix.h"

namespace PS {

// Abstract base class for encapsulating a loss object. For any new added loss,
// one should add the according entry in proto/app.proto and the factory class LossFactory
template<typename T>
class Loss {
 public:
  // evaluate the loss value
  virtual T evaluate(const MatrixPtrList<T>& data) = 0;

  // compute the gradients
  virtual void compute(const MatrixPtrList<T>& data, MatrixPtrList<T> gradients) = 0;
};

template<typename T> using LossPtr = std::shared_ptr<Loss<T>>;

} // namespace PS
