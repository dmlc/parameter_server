#pragma once
#include "penalty/penalty.h"
#include "base/shared_array.h"
namespace PS {

// lambda * ||w||_p^P = lambda * \sum_i w_i^p
// TODO infinity
template <typename T>
class PNormPenalty : public Penalty<T> {
 public:
  PNormPenalty(T p, T lambda) : p_(p), lambda_(lambda) {
    CHECK_GE(p_, 0);
    CHECK_GE(lambda_, 0);
  }
  bool smooth() { return p_ > 1; }

  T evaluate(const MatrixPtr<T>& model) {
    auto w = model->value().eigenArray();
    return lambda_ * pow(w.abs(), p_).sum();
  }

  T lambda() { return lambda_; }
  T p() { return p_; }
 private:
  T p_;
  T lambda_;
};

} // namespace PS
