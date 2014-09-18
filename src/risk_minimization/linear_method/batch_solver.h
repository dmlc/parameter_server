#pragma once
#include "risk_minimization/linear_method/linear_method.h"

namespace PS {
namespace LM {

class BatchSolver : public LinearMethod {
 public:
  virtual void init();
  virtual void run();

 protected:
  static const int kPace = 10;

  virtual int loadData(const MessageCPtr& msg, InstanceInfo* info);
  virtual void preprocessData(const MessageCPtr& msg);
  virtual void updateModel(const MessagePtr& msg);
  virtual void runIteration();

  virtual RiskMinProgress evaluateProgress();
  virtual void showProgress(int iter);

  void computeEvaluationAUC(AUCData *data);
  void saveModel(const MessageCPtr& msg);

  bool loadCache(const string& cache_name);
  bool saveCache(const string& cache_name);

  typedef shared_ptr<KVVector<Key, double>> KVVectorPtr;
  KVVectorPtr w_;

  // feature block info, only available at the scheduler, format: pair<fea_grp_id, fea_range>
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blocks_;
  std::vector<int> block_order_;
  std::vector<int> prior_block_order_;

  // global data information, only available at the scheduler
  InstanceInfo g_train_ins_info_;

  // training data, available at the workers
  // MatrixPtrList<double> train_data_;
  std::map<int, MatrixPtr<double>> X_;
  MatrixPtr<double> y_;

  // std::map<int, int> grp_map_;
  // mapping a feature group into a channel
  std::map<int, int> grp2chl_;

  // dual_ = X_ * w_
  SArray<double> dual_;

  std::mutex mu_;
};


} // namespace LM
} // namespace PS

  // void saveAsDenseData(const Message& msg);
