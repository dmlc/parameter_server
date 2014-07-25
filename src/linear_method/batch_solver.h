#progma once

#include "linear_method/block_solver.h"

namespace PS {
namespace LM {

class BatchSolver : public BlockSolver {
 public:
  virtual void init();
  virtual void run();

 protected:
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;

  void saveModel(const Message& msg);
  virtual void computeEvaluationAUC(AUCData *data) { }
  virtual RiskMinProgress evaluateProgress();

  virtual void prepareData(const Message& msg);
  virtual void updateModel(Message* msg);

  void showProgress(int iter);

  typedef shared_ptr<KVVector<Key, double>> KVVectorPtr;
  KVVectorPtr w_;

  // dual_ = X_ * w_ or dual_ = exp(X_*w_)
  SArray<double> dual_;
  std::mutex mu_;
};


} // namespace LM
} // namespace PS
