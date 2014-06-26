#pragma once
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

  RiskMinProgress evaluateProgress() {
    auto prog = LinearBlockIterator::evaluateProgress();
    if (exec_.isServer()) {
      prog.set_violation(violation_);
      prog.set_nnz_active_set(active_set_.nnz());
    }
    return prog;
  }

  void showProgress(int iter);
  void showKKTFilter(int iter);
  constexpr static double kInactiveValue_ = 65536;

  double KKT_filter_threshold_;
  double violation_;
};

} // namespace PS
