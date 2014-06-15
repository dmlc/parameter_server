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

  std::map<int, size_t> Xs_col_offset_;
  SArray<double> Xw_;
  std::mutex mu_;
};

} // namespace PS
