#pragma once
#include "linear_method/batch_worker.h"
namespace PS {
namespace LM {

class DarlinWorker : public BatchWorker {
 public:
  void computeGradient(int time, const MessagePtr& msg);
  void preprocessData(int time, const Call& cmd);

 protected:
  void computeAndPushGradient(int time, Range<Key> g_key_range, int grp, SizeR col_range);
  void computeGradient(int grp, SizeR col_range, SArray<double> G, SArray<double> U);
  void updateDual(int grp, SizeR col_range, SArray<double> new_weight);
  void updateDual(int grp, SizeR row_range, SizeR col_range, SArray<double> w_delta);


  std::unordered_map<int, Bitmap> active_set_;
  std::unordered_map<int, SArray<double>> delta_;
  SparseFilter kkt_filter_;
};

} // namespace LM
} // namespace PS
