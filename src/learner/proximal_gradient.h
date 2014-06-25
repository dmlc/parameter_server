#pragma once
#include "learner/aggregate_gradient.h"
#include "penalty/p_norm_penalty.h"

namespace PS {

template <typename T>
class ProximalGradient : public AggGradLearner<T> {
 public:

  // first-order gradients and diag hessian
  void compute(const MatrixPtrList<T>& data,
               const AggGradLearnerArg& arg,
               SArrayList<T> gradients) {
    CHECK_GE(gradients.size(), 2);
    MatrixPtrList<T> ptrs = {gradients[0].matrix(), gradients[1].matrix()};
    this->loss_->compute(data, ptrs);
  }

  // update weight
  void update(const SArrayList<T>& gradients,
              const AggGradLearnerArg& arg,
              SArray<T> weight) {
    CHECK_GE(gradients.size(), 2);
    auto G = gradients[0];
    auto U = gradients[1];
    CHECK_EQ(G.size(), weight.size());
    CHECK_EQ(U.size(), weight.size());

    auto penalty = std::static_pointer_cast<PNormPenalty<T>>(this->penalty_);

    if (penalty->p() == 1) {
      // soft-shrinkage
      for (int i = 0; i < weight.size(); ++i) {
        T g_pos = G[i] + penalty->lambda();
        T g_neg = G[i] - penalty->lambda();
        T w = weight[i];

        T d = - w;
        T u = U[i] / (T)arg.learning_rate() + 1e-10;

        if (g_pos <= u * w) {
          d = - g_pos / u;
        } else if (g_neg >= u * w) {
          d = - g_neg / u;
        }

        d = std::min(5.0, std::max(-5.0, d));
        weight[i] += d;
      }
    } else {
      // TODO
    }
  }
};

} // namespace PS
