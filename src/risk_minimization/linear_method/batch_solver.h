#pragma once

#include "risk_minimization/linear_method/block_solver.h"

namespace PS {
namespace LM {

class BatchSolver : public LinearMethod {
 public:
  virtual void init();
  virtual void run();

 protected:
  virtual InstanceInfo prepareData(const MessagePtr& msg);
  virtual void updateModel(const MessagePtr& msg);
  virtual void runIteration();

  virtual RiskMinProgress evaluateProgress();
  virtual void showProgress(int iter);

  void computeEvaluationAUC(AUCData *data);
  void saveModel(const MessagePtr& msg);
  // void saveAsDenseData(const Message& msg);

  bool loadCache(const string& cache_name);
  bool saveCache(const string& cache_name);

  // void loadData(const DataConfig& data, const string& cache_name);

  typedef shared_ptr<KVVector<Key, double>> KVVectorPtr;
  KVVectorPtr w_;

  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blocks_;
  std::vector<int> block_order_;
  std::vector<int> prior_block_order_;

  // global data information, only available at the scheduler
  InstanceInfo g_train_ins_info_;
  // training data, available at the workers
  MatrixPtr<double> y_, X_;
  // dual_ = X_ * w_
  SArray<double> dual_;

  std::mutex mu_;
};


} // namespace LM
} // namespace PS
