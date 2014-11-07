#pragma once

#include "base/shared_array.h"
#include "util/parallel_sort.h"

namespace PS {

// evaluation in a single machine
template <typename V>
class Evaluation {
 public:
  static V auc(const SArray<V>& label, const SArray<V>& predict);


};

template <typename V>
V Evaluation<V>::auc(const SArray<V>& label, const SArray<V>& predict) {
  int n = label.size();
  CHECK_EQ(n, predict.size());
  struct Entry {
    V label;
    V predict;
  };
  SArray<Entry> buff(n);
  for (int i = 0; i < n; ++i) {
    buff[i].label = label[i];
    buff[i].predict = predict[i];
  }
  // parallelSort(buff.data(), n, FLAGS_num_threads, [](
  //     const Entry& a, const Entry&b) { return a.predict < b.predict; });
  std::sort(buff.data(), buff.data()+n,  [](const Entry& a, const Entry&b) {
      return a.predict < b.predict; });
  V area = 0, cum_tp = 0;
  for (int i = 0; i < n; ++i) {
    if (buff[i].label > 0) {
      cum_tp += 1;
    } else {
      area += cum_tp;
    }
  }
  area /= cum_tp * (n - cum_tp);
  return area < 0.5 ? 1 - area : area;
}

} // namespace PS
