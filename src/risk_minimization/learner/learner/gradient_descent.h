#pragma once
#include "learner/aggregate_gradient.h"

namespace PS {

template <typename T>
class GradientDescent : public AggGradLearner<T> {
 public:

  // only first-order gradients
  void compute(const MatrixPtrList<T>& data,
               const AggGradLearnerArg& arg,
               SArrayList<T> gradients) {
    MatrixPtrList<T> ptrs = {gradients[0].matrix()};
    this->loss_->compute(data, ptrs);
  }

  // update weight
  void update(const SArrayList<T>& gradients,
              const AggGradLearnerArg& arg,
              SArray<T> weight) {
    CHECK_EQ(gradients[0].size(), weight.size());
    weight.eigenVector() -= arg.learning_rate() * gradients[0].eigenVector();
  }
};

} // namespace PS
