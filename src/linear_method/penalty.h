#pragma once
#include "util/common.h"
#include "base/matrix.h"
#include "linear_method/linear_method.pb.h"

namespace PS {
namespace LM {

template<typename T> class Penalty;
template<typename T> using PenaltyPtr = std::shared_ptr<Penalty<T>>;
template<typename T> class PNormPenalty;

template<typename T> class Penalty {
 public:
  static PenaltyPtr<T> create(const PenaltyConfig& config) {
    switch (config.type()) {
      case PenaltyConfig::L1:
        return PenaltyPtr<T>(new PNormPenalty<T>(1, config.lambda(0)));
      case PenaltyConfig::L2:
        return PenaltyPtr<T>(new PNormPenalty<T>(2, config.lambda(0)));
      default:
        CHECK(false) << "unknown type: " << config.DebugString();
    }
    return PenaltyPtr<T>(nullptr);
  }
  virtual T evaluate(const MatrixPtr<T>& model) = 0;
};

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

} // namespace LM
} // namespace PS
