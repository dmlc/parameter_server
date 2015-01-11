#pragma once
#include "learner/sgd_server.h"
namespace PS {
namespace LM {

template <typename V>
V softThresholding(V x, V lambda_1, V lambda_2) {
  if (x > 0) {
    return x > lambda_1 ? (x - lambda_1) / lambda_2 : 0;
  } else {
    return x < - lambda_1 ? (x + lambda_1) / lambda_2 : 0;
  }
}

template <typename V>
struct FTRLEntry {
  V w = 0;  // not necessary to store w, because it can be computed from z
  V z = 0;
  V sqrt_n = 0;

  // learning rate
  static V alpha, beta;

  // penalty
  static V lambda1 = 0, lambda2 = 0;

  // status
  static V norm1 = 0;
  static V norm2 = 0;
  static size_t nnz = 0;

  void get(char const* data) {
    // update model
    V w_old = w;
    V grad = *((V*)data);
    V sqrt_n_new = sqrt(sqrt_n * sqrt_n + grad * grad);
    V sigma = (sqrt_n_new - sqrt_n) / alpha;
    z += grad  - sigma * w;
    sqrt_n = sqrt_n_new;
    w = - softThresholding(
        z, lambda1, lambda2 + (beta + sqrt_n_new) / alpha);

    // update status
    norm1 += fabs(w) - fabs(w_old);
    norm2 += w * w - w_old * w_old;
    if (w == 0 && w_old != 0) {
      -- nnz;
    } else if (w != 0 && w_old == 0) {
      ++ nnz;
    }
  }

  void set(char* data) {
    *((V*)data) = w;
  }
};

template <typename V>
class FTRLServer : public SGDServer<FTRLEntry<V>> {
public:
  void init() {
    // init static variables
  }

  void evaluate(SGDProgress* prog) {
    typedef FTRLEntry<V> E;
    prog->set_objv(E::norm1 * E::lambda1 + .5 * E::lambda2 * sqrt(E::norm2));
    prog->set_nnz_w(E::nnz);
  }

  void saveModel() {
  }
};


} // namespace LM
} // namespace PS
