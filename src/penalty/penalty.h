#pragma once
#include "util/common.h"
#include "base/matrix.h"

namespace PS {

template<typename T>
class Penalty {
 public:
  virtual T evaluate(const MatrixPtr<T>& model) = 0;
};

template<typename T> using PenaltyPtr = std::shared_ptr<Penalty<T>>;

} // namespace PS
