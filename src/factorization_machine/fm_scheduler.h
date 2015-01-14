#pragma once
// #include "earner/sgd_scheduler.h"
namespace PS {
namespace FM {

class FMScheduler : public SGDScheduler {
 public:
  virtual void init() { }
  virtual void run() { }
};
} // namespace FM
} // namespace PS
