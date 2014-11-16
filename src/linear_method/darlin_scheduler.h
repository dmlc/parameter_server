#pragma once
#include "linear_method/batch_scheduler.h"
#include "linear_method/darlin_common.h"
namespace PS {
namespace LM {

class DarlinScheduler : public BatchScheduler, public DarlinCommon {
 public:
  virtual void runIteration();
  virtual void showProgress(int iter);
 protected:
  void showKKTFilter(int iter);

  double kkt_filter_threshold_;
};

} // namespace LM
} // namespace PS
