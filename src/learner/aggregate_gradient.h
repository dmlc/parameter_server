#pragma once
#include "learner/optimization.h"
#include "base/matrix.h"
#include "base/shared_array_inl.h"
#include "proto/config.pb.h"
#include "loss/loss_factory.h"
#include "penalty/penalty_factory.h"
namespace PS {

template <typename T>
class AggGradLearner : public Optimization<T> {
 public:
  void setLoss(const LossPtr<T>& loss) { loss_ = loss; }
  void setPenalty(const PenaltyPtr<T>& penalty) { penalty_ = penalty; }

  // both gradients and weight should be pre-allocated
  // compute the gradient
  virtual void compute(const MatrixPtrList<T>& data,
                       const AggGradLearnerArg& arg,
                       SArrayList<T> gradients) = 0;
  // update weight
  virtual void update(const SArrayList<T>& gradients,
                      const AggGradLearnerArg& arg,
                      SArray<T> weight) = 0;
 protected:
  LossPtr<T> loss_;
  PenaltyPtr<T> penalty_;
};

template<typename T> using AggGradLearnerPtr =
    std::shared_ptr<AggGradLearner<T> >;

} // namespace PS

  // wrappers
  // MatrixPtrInitList<T>
  // compute(const MatrixPtrInitList<T>& data, const AggGradLearnerArg& arg) {

  //   std::vector<MatrixPtr> ptrs;
  //   for (auto& d : data) ptrs.push_back(d);
  //   return compute(ptrs, arg);
  // }

  // void update(const initializer_list<MatrixPtr>& gradients,
  //             const AggGradLearnerArg& arg,
  //             MatrixPtr weight) {
  //   std::vector<MatrixPtr> ptrs;
  //   for (auto& g : gradients) ptrs.push_back(g);
  //   return update(ptrs, arg, weight);
  // }
