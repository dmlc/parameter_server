#pragma once
#include "risk_minimization/risk_minimization.h"
#include "parameter/kv_vector.h"

#include "risk_minimization/loss_inl.h"
#include "risk_minimization/penalty_inl.h"
#include "risk_minimization/learner/learner.h"
#include "risk_minimization/learner/aggregate_gradient.h"

// #include "loss/scalar_loss.h"
// #include "penalty/p_norm_penalty.h"

namespace PS {
namespace LM {

// linear classification/regerssion
class LinearMethod : public RiskMinimization {

 public:
  virtual void init();

 protected:
  void startSystem();

  LossPtr<double> loss_;
  PenaltyPtr<double> penalty_;
  shared_ptr<AggGradLearner<double>> learner_;

};

} // namespace LM

} // namespace PS
