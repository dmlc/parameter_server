#pragma once
#include <float.h>
#include "linear_method/batch_solver.h"
#include "base/bitmap.h"

namespace PS {

// optimizated for sparse logisitic regression
class BlockCoordinateL1LR : public BatchSolver {
 public:
  virtual void run();
 protected:
  virtual void prepareData(const Message& msg);
  virtual void updateModel(Message* msg);


  SArrayList<double> computeGradients(SizeR local_fea_range);
  void updateDual(SizeR local_fea_range, SArray<double> new_weight);
  void updateWeight(
      SizeR local_fea_range, const SArray<double>& G, const SArray<double>& U);

  void computeEvaluationAUC(AUCData *data);
  RiskMinProgress evaluateProgress();

  void showProgress(int iter);
  void showKKTFilter(int iter);

  void computeGradients(SizeR local_fea_range, SArray<double> G, SArray<double> U);
  void updateDual(
      SizeR local_ins_range, SizeR local_fea_range, const SArray<double>& w_delta);

  Bitmap active_set_;
  SArray<double> delta_;

  // snappy has good compression rate on 0xffff..ff, it is nan for double
  const double kInactiveValue_ = *((double*)&kuint64max);

  double KKT_filter_threshold_;
  double violation_;

  BlockCoordL1LRConfig l1lr_cf_;
};

} // namespace PS
