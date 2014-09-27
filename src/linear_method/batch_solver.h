#pragma once
#include "linear_method/linear_method.h"
#include "data/slot_reader.h"

namespace PS {
namespace LM {

class BatchSolver : public LinearMethod {
 public:
  virtual void init();
  virtual void run();

 protected:
  static const int kPace = 10;

  virtual int loadData(const MessageCPtr& msg, ExampleInfo* info);
  virtual void preprocessData(const MessageCPtr& msg);
  virtual void updateModel(const MessagePtr& msg);
  virtual void runIteration();

  virtual Progress evaluateProgress();
  virtual void showProgress(int iter);

  void computeEvaluationAUC(AUCData *data);
  void saveModel(const MessageCPtr& msg);

  bool loadCache(const string& name) { return dataCache(name, true); }
  bool saveCache(const string& name) { return dataCache(name, false); }
  bool dataCache(const string& name, bool load);

  typedef shared_ptr<KVVector<Key, double>> KVVectorPtr;
  KVVectorPtr w_;

  // feature block info, only available at the scheduler, format: pair<fea_grp_id, fea_range>
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blk_;
  std::vector<int> blk_order_;
  std::vector<int> prior_blk_order_;
  std::vector<int> fea_grp_;

  // global data information, only available at the scheduler
  ExampleInfo g_train_info_;

  // training data, available at the workers
  std::map<int, MatrixPtr<double>> X_;
  MatrixPtr<double> y_;
  SlotReader slot_reader_;
  // dual_ = X * w
  SArray<double> dual_;


  std::mutex mu_;
};


} // namespace LM
} // namespace PS

  // void saveAsDenseData(const Message& msg);
