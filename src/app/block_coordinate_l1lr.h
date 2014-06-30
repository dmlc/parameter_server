#pragma once
#include <float.h>
#include "app/linear_block_iterator.h"
#include "base/bitmap.h"

namespace PS {

// optimizated for sparse logisitic regression
class BlockCoordinateL1LR : public LinearBlockIterator {
 public:
  virtual void run();
 protected:
  virtual void prepareData(const Message& msg);
  virtual void updateModel(Message* msg);

  Bitmap active_set_;
  SArray<double> delta_;

  void computeGradients(SizeR local_feature_range, SArray<double> G, SArray<double> U);

  void updateDual(SizeR local_example_range, SizeR local_feature_range, SArray<double> w_delta);

  void updateWeight(SizeR local_feature_range, const SArray<double>& G, const SArray<double>& U);

  RiskMinProgress evaluateProgress();

  void showProgress(int iter);
  void showKKTFilter(int iter);

  // snappy has good compression rate on 0xffff..ff
  const double kInactiveValue_ = *((double*)&kuint64max);

  double KKT_filter_threshold_;
  double violation_;

  BlockCoordL1LRConfig l1lr_cf_;
};

} // namespace PS
