#pragma once
#include "parameter/shared_parameter.h"
#include "linear_method/online_sovler.h"
#include "linear_method/ftrl_model.h"
#include "linear_method/loss_inl.h"

namespace PS {
namespace LM {

// Online gradient descent optimized for l_1-regularized logistic regression. It
// has been used by Google's Smartass, more details are in the paper
//
// H. McMahan. et.al, Ad Click Prediction: a View from the Trenches, KDD'13
//
class FTRL : public OnlineSolver {
 public:
  void init();
  void run();

  void saveModel(const MessageCPtr& msg);
  void updateModel(const MessagePtr& msg);
 protected:
  typedef double Real;
  void countKeys(const MatrixPtr<Real>& Y, const MatrixPtr<Real>& X,
                 SArray<uint32>* pos, SArray<uint32>* neg);

  // void computeGradient();

  SharedParameterPtr<Key> model_;
  LossPtr<Real> loss_;
};

} // namespace LM
} // namespace PS
