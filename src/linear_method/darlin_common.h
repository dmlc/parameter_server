#pragma once
#include "base/bitmap.h"
#include "filter/sparse_filter.h"
namespace PS {
namespace LM {

class DarlinCommon {
 protected:
  std::unordered_map<int, Bitmap> active_set_;
  std::unordered_map<int, SArray<double>> delta_;
  SparseFilter kkt_filter_;
};

} // namespace LM
} // namespace PS
