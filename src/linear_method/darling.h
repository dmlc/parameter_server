#pragma once
#include <float.h>
#include "linear_method/batch_solver.h"
#include "base/bitmap.h"

namespace PS {
namespace LM {

// batch algorithm for sparse logistic regression
class Darling : public BatchSolver {
 public:
  virtual void init();
 protected:
  virtual void runIteration();
  virtual void preprocessData(const MessageCPtr& msg);
  virtual void updateModel(const MessagePtr& msg);

  SArrayList<double> computeGradients(int grp, SizeR col_range);
  void updateDual(int grp, SizeR col_range, SArray<double> new_weight);
  void updateWeight(int grp, SizeR col_range, SArray<double> G, SArray<double> U);

  Progress evaluateProgress();
  void showProgress(int iter);
  void showKKTFilter(int iter);

  void computeGradients(int grp, SizeR col_range, SArray<double> G, SArray<double> U);
  void updateDual(int grp, SizeR row_range, SizeR col_range, SArray<double> w_delta);

  double newDelta(double delta_w) {
    return std::min(conf_.darling().delta_max_value(), 2 * fabs(delta_w) + .1);
  }

  bool inactive(double val) { return val != val; }
  double kInactiveValue_;

  std::unordered_map<int, Bitmap> active_set_;
  std::unordered_map<int, SArray<double>> delta_;

  double KKT_filter_threshold_;
  double violation_;

  DarlingConfig darling_conf_;
};

} // namespace LM
} // namespace PS
