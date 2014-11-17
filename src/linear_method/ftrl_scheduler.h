#pragma once
#include "linear_method/scheduler.h"
namespace PS {
namespace LM {

class FTRLScheduler : public Scheduler {
 public:
  virtual void init() { Scheduler::init(); }
  virtual void run();

 protected:
  void showProgress();
  unique_ptr<std::thread> monitor_thr_;

  size_t num_ex_processed_ = 0;
};


} // namespace LM
} // namespace PS
