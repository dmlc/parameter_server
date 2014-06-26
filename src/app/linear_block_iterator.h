#pragma once
#include "app/linear_method.h"

namespace PS {

// updates a block of weights by a block of training data
class LinearBlockIterator : public LinearMethod {

 public:
  virtual void run();

 protected:
  typedef std::vector<std::pair<int, Range<Key>>> FeatureBlocks;


  virtual RiskMinProgress evaluateProgress();

  virtual void prepareData(const Message& msg);
  virtual void updateModel(Message* msg);

  void showProgress(int iter);

  FeatureBlocks partitionFeatures();
  // dual_ = X_ * w_ or dual_ = exp(X_*w_)
  SArray<double> dual_;
  std::mutex mu_;
};

} // namespace PS
