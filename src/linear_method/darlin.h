#pragma once
#include "linear_method/batch_solver.h"
namespace PS {
namespace LM {

class Darlin : public BatchSolver {
 public:
  virtual void runIteration();
  virtual void preprocessData(const MessageCPtr& msg);
  virtual void updateModel(const MessagePtr& msg);
 protected:

  Progress evaluateProgress();
  void showProgress(int iter);
  void showKKTFilter(int iter);
}

} // namespace LM
} // namespace PS
