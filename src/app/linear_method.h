#pragma once
#include "app/risk_minimization.h"
#include "parameter/kv_vector.h"

#include "loss/loss_factory.h"
#include "penalty/penalty_factory.h"
#include "learner/learner_factory.h"

// #include "loss/scalar_loss.h"
// #include "penalty/p_norm_penalty.h"

namespace PS {

// linear classification/regerssion

class LinearMethod : public RiskMinimization {

 public:
  void init();

 protected:

  void startSystem();
  void saveModel(const Message& msg);

 protected:
  typedef shared_ptr<KVVector<Key, double>> KVVectorPtr;
  KVVectorPtr w_;

  LossPtr<double> loss_;
  PenaltyPtr<double> penalty_;
  shared_ptr<AggGradLearner<double>> learner_;

  // training data
  MatrixPtr<double> y_, X_;
  SArray<double> Xw_;
  // std::map<int, MatrixPtr<double>> Xs_;

};

} // namespace PS
