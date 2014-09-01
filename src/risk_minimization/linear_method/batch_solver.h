#pragma once

#include "risk_minimization/linear_method/block_solver.h"

namespace PS {
namespace LM {

class BatchSolver : public LinearMethod {
 public:
  virtual void init();
  virtual void run();

 protected:
  virtual InstanceInfo prepareData(const Message& msg);
  virtual void updateModel(Message* msg);
  virtual void runIteration();

  virtual RiskMinProgress evaluateProgress();
  virtual void showProgress(int iter);

  void computeEvaluationAUC(AUCData *data);
  void saveModel(const Message& msg);
  void saveAsDenseData(const Message& msg);

  void loadData(const DataConfig& data, const string& cache_name);

  // training data, available at the workers
  MatrixPtr<double> y_, X_;

  typedef shared_ptr<KVVector<Key, double>> KVVectorPtr;
  KVVectorPtr w_;

  // global data information, only available at the scheduler
  InstanceInfo g_train_ins_info_;

  // std::vector<MatrixInfo> g_training_info_, g_validation_info_;
  // size_t g_num_training_ins_ = 0;
  // Range<Key> g_fea_range_;

  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blocks_;
  std::vector<int> block_order_;

  // dual_ = X_ * w_
  SArray<double> dual_;
  std::mutex mu_;
};


} // namespace LM
} // namespace PS
