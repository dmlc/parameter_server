#pragma once
#include "util/common.h"
#include "util/matrix.h"
#include "app/linear_method/proto/linear.pb.h"
namespace PS {
namespace LM {

/**
 * @brief Interface for the penalty
 */
template<typename T> class Penalty {
 public:
  Penalty() { }
  virtual ~Penalty() { }
  /**
   * @brief evaluate the objective
   *
   * @param model
   *
   * @return objective value
   */
  virtual T eval(const MatrixPtr<T>& model) = 0;

  /**
   * @brief Solve the proximal operator
   *
   * \f$ \argmin_x 0.5/\eta (x - z)^2 + h(x)\f$, where h denote this penatly, and in
   * proximal gradient descent, z = w - eta * grad
   *
   * @param z
   * @param eta
   * @return
   */
  virtual T proximal(T z, T eta) = 0;
};

/**
 * @brief \f$ \lambda_1 * \|x\|_1 + \lambda_2 * \|x\|_2^2 \f$
 */
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
    return z > 0 ? (z - leta) / ( 1 + lambda2_ * eta) : (z + leta) / ( 1 + lambda2_ * eta);
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
  CHECK_GE(conf.lambda_size(), 1);
  switch (conf.type()) {
    case PenaltyConfig::L1: {
      T l1 = conf.lambda(0);
      T l2 = conf.lambda_size() > 1 ? conf.lambda(1) : 0;
      return new ElasticNet<T>(l1, l2);
    }
    case PenaltyConfig::L2:
      return new ElasticNet<T>(0, conf.lambda(0));
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
//     auto w = model->value().EigenArray();
//     return lambda_ * pow(w.abs(), p_).sum();
//   }

//   T lambda() { return lambda_; }
//   T p() { return p_; }
//  private:
//   T p_;
//   T lambda_;
// };
