#pragma once
#include "linear_method/batch_server.h"
#include "base/bitmap.h"
#include "filter/sparse_filter.h"
namespace PS {
namespace LM {

class DarlinServer : BatchServer {
 public:

  void updateWeight(const Call& cmd);
 protected:
  double newDelta(double delta_w) {
    return std::min(conf_.darling().delta_max_value(), 2 * fabs(delta_w) + .1);
  }

  std::unordered_map<int, Bitmap> active_set_;
  std::unordered_map<int, SArray<double>> delta_;

  SparseFilter kkt_filter_;

  double kkt_filter_threshold_;
  double violation_;
};
} // namespace LM
} // namespace PS
