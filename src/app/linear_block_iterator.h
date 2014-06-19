#pragma once
#include "app/linear_method.h"

namespace PS {

// updates a block of weights by a block of training data
class LinearBlockIterator : public LinearMethod {

 public:
  void run();

 protected:

  RiskMinProgress evaluateProgress();
  void updateXw();

  void prepareData(const Message& msg);
  void updateModel(Message* msg);
 private:
  // Xw_ = X_ * w_
  SArray<double> Xw_;
  std::mutex mu_;
};

} // namespace PS
