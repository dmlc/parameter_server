#pragma once

#include "linear_method/block_solver.h"

namespace PS {
namespace LM {

class BatchSolver : public BlockSolver {
 public:
  virtual void init();
  virtual void run();

 protected:
  virtual void prepareData(const Message& msg);
  virtual void updateModel(Message* msg);

  virtual RiskMinProgress evaluateProgress();
  virtual void showProgress(int iter);

  void computeEvaluationAUC(AUCData *data);
  void saveModel(const Message& msg);

  typedef shared_ptr<KVVector<Key, double>> KVVectorPtr;
  KVVectorPtr w_;

  // dual_ = X_ * w_
  SArray<double> dual_;
  std::mutex mu_;
};


} // namespace LM
} // namespace PS
