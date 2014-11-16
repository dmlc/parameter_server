#pragma once
#include "linear_method/batch_scheduler.h"
namespace PS {
namespace LM {

class DarlinScheduler : public BatchScheduler {
 public:
  virtual void runIteration();
  virtual void showProgress(int iter);
 protected:
  void showKKTFilter(int iter);
}

} // namespace LM
} // namespace PS
