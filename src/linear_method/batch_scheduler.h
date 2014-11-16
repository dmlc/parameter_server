#pragma once
#include "linear_method/scheduler.h"
#include "linear_method/batch_common.h"
namespace PS {
namespace LM {

class BatchScheduler : public Scheduler, public BatchCommon {
 public:
  virtual void init();
  virtual void run();

 protected:
  virtual void runIteration();
  virtual void showProgress(int iter);

  // feature block info, format: pair<fea_grp_id, fea_range>
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;
  FeatureBlocks fea_blk_;
  std::vector<int> blk_order_;
  std::vector<int> prior_blk_order_;
  // global data information
  ExampleInfo g_train_info_;
};


} // namespace LM
} // namespace PS
