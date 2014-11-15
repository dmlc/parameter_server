#pragma once
#include "linear_method/linear_method.h"
namespace PS {
namespace LM {

class BatchWorker;
class BatchSolver;
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

  std::shared_ptr<BatchWorker> worker_;
  std::shared_ptr<BatchServer> server_;

  // feature block info, format: pair<fea_grp_id, fea_range>
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blk_;
  std::vector<int> blk_order_;
  std::vector<int> prior_blk_order_;
  std::vector<int> fea_grp_;

  // global data information
  ExampleInfo g_train_info_;

};


} // namespace LM
} // namespace PS

  // void saveAsDenseData(const Message& msg);
