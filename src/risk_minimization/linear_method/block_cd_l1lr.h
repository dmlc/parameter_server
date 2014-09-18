#pragma once
#include <float.h>
#include "risk_minimization/linear_method/batch_solver.h"
#include "base/bitmap.h"

namespace PS {
namespace LM {

// optimizated for sparse logisitic regression
class BlockCoordDescL1LR : public BatchSolver {
 public:
 protected:
  virtual void runIteration();
  virtual InstanceInfo prepareData(const MessagePtr& msg);
  virtual void updateModel(const MessagePtr& msg);

  SArrayList<double> computeGradients(SizeR col_range);
  void updateDual(int ch, SizeR col_range, SArray<double> new_weight);
  void updateWeight(int ch, SizeR col_range, SArray<double> G, SArray<double> U);

  RiskMinProgress evaluateProgress();
  void showProgress(int iter);
  void showKKTFilter(int iter);

  void computeGradients(SizeR col_range, SArray<double> G, SArray<double> U);
  void updateDual(SizeR row_range, SizeR col_range, SArray<double> w_delta);

  double newDelta(double delta_w) {
    return std::min(bcd_l1lr_cf_.delta_max_value(), 2 * fabs(delta_w) + .1);
  }

  Bitmap active_set_;
  SArray<double> delta_;

  // snappy has good compression rate on 0xffff..ff, it is nan for double
  const double kInactiveValue_ = *((double*)&kuint64max);
  // const double kInactiveValue_ = 0;

  double KKT_filter_threshold_;
  double violation_;

  BCDL1LRConfig bcd_l1lr_cf_;
};

} // namespace LM
} // namespace PS
