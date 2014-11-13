#pragma once
#include "parameter/shared_parameter.h"
#include "linear_method/online_solver.h"
#include "linear_method/ftrl_model.h"
#include "linear_method/loss_inl.h"
#include "util/threadsafe_limited_queue.h"
namespace PS {
namespace LM {

// Online gradient descent optimized for l_1-regularized logistic regression. It
// has been used by Google's Smartass, more details are in the paper
//
// H. McMahan. et.al, Ad Click Prediction: a View from the Trenches, KDD'13
//
typedef double Real;
class FTRLWorker;
class FTRLServer;

class FTRL : public OnlineSolver {
 public:
  void init();
  void run();

  void saveModel(const MessageCPtr& msg);
  void updateModel(const MessagePtr& msg);

 protected:
  void showProgress();
  FTRLWorker* worker_ = nullptr;
  FTRLServer* server_ = nullptr;

  unique_ptr<std::thread> prog_thr_;
};

} // namespace LM
} // namespace PS
