#pragma once
#include "util/common.h"
#include "util/matrix.h"
#include "linear_method/proto/lm.pb.h"
namespace PS {
namespace LM {

// interface for the penalty
template<typename T> class Penalty {
 public:
  Penalty() { }
  virtual ~Penalty() { }
  // evaluate the objective
  virtual T eval(const MatrixPtr<T>& model) = 0;

  // solve the proximal operater
  //  argmin_x 0.5/eta (x - z)^2 + h(x), where h denote this penatly
  // in proximal gradient descent, z = w - eta * grad
  virtual T proximal(T z, T eta) = 0;
};





// lambda1 * |x|_1 + lambda2 * |x|_2
template <typename T>
class ElasticNet : public Penalty<T> {
 public:
  ElasticNet(T lambda1, T lambda2) : lambda1_(lambda1), lambda2_(lambda2) {
    CHECK_GE(lambda1, 0);
    CHECK_GE(lambda2, 0);
  }
  ~ElasticNet() { }

  T eval(const MatrixPtr<T>& model) { return 0; }  // TODO

  T proximal(T z, T eta) {
    CHECK_GT(eta, 0);
    T leta = lambda1_ * eta;
    if (z <= leta && z >= -leta) return 0;
    if (z > 0) {
      return z > leta ? (z - leta) / ( 1 + lambda2_ * eta) : 0;
    } else {
      return z < -leta ? (z + leta) / ( 1 + lambda2_ * eta) : 0;
    }
  }
 private:
  T lambda1_, lambda2_;
};


// template <typename T>
// class L2 : public Penalty<T> {
//  public:
//   L2(T lambda) : lambda_(lambda) { }
//     CHECK_GE(lambda, 0);
//   }
//   ~L2() { }
//  private:
//   T evaluate(const MatrixPtr<T>& model) { return 0; }  // TODO
//   T proximal(T z, T eta) {
//   }
// T lambda_;
// };

template<typename T>
Penalty<T>* createPenalty(const PenaltyConfig& conf) {
  CHECK_GE(conf.coef_size(), 1);
  switch (conf.type()) {
    case PenaltyConfig::L1: {
      T l1 = conf.coef(0);
      T l2 = conf.coef_size() > 1 ? conf.coef(1) : 0;
      return new ElasticNet<T>(l1, l2);
    }
    case PenaltyConfig::L2:
      return new ElasticNet<T>(0, conf.coef(0));
    default:
      CHECK(false) << "unknown type: " << conf.DebugString();
  }
  return nullptr;
}

} // namespace LM
} // namespace PS

// // lambda * ||w||_p^P = lambda * \sum_i w_i^p
// // TODO infinity
// template <typename T>
// class PNormPenalty : public Penalty<T> {
//  public:
//   PNormPenalty(T p, T lambda) : p_(p), lambda_(lambda) {
//     CHECK_GE(p_, 0);
//     CHECK_GE(lambda_, 0);
//   }
//   bool smooth() { return p_ > 1; }

//   T evaluate(const MatrixPtr<T>& model) {
//     auto w = model->value().eigenArray();
//     return lambda_ * pow(w.abs(), p_).sum();
//   }

//   T lambda() { return lambda_; }
//   T p() { return p_; }
//  private:
//   T p_;
//   T lambda_;
// };
