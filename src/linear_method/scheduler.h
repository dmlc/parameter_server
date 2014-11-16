#pragma once
#include "linear_method/linear_method.h"
#include "base/auc.h"
namespace PS {
namespace LM {
// the base class for the scheduler
class Scheduler : public LinearMethod {
 public:
  virtual void init() { LinearMethod::init(); }
  virtual void process(const MessagePtr& msg);

  void mergeProgress(int iter);
  void mergeAUC(AUC* auc);
  void startSystem();
 protected:
  void showTime(int iter);
  void showObjective(int iter);
  void showNNZ(int iter);

  // progress of all iterations, only valid for the scheduler. The progress of
  // all nodes are merged for every iteration. It's for batch algorithms.
  std::map<int, Progress> g_progress_;
  // recent progress for every node. It's for online algorithms.
  std::map<NodeID, Progress> recent_progress_;
  std::mutex progress_mu_;
};

} // namespace LM
} // namespace PS
