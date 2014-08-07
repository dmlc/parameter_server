#pragma once

#include "neural_network/loss_layer.h"
namespace PS {
namespace NN {

template<typename V>
class AUCLayer : public LossLayer<V> {
 public:
  V forward() {
    this->checkValue(2);
    // predict
    auto X = this->in_args_[0]->value->eigenArray();
    // label
    auto Y = this->in_args_[1]->value->eigenArray();

    typedef std::pair<V,V> Pair;
    size_t n = X.size();
    std::vector<Pair> data(n);
    for (int i = 0; i < n; ++i) {
      data[i].first = X[i];
      data[i].second = Y[i];
    }

    std::sort(data.begin(), data.end(), [](const Pair& a, const Pair& b)->bool {
        return (a.first < b.first);
      });

    V tp = 0, area = 0;
    for (size_t i = 0; i < n; ++i) {
      if (data[i].second == 1) {
        tp += 1;
      } else {
        area += tp;
      }
    }
    area = area / tp / (n - tp);
    return std::max(area, 1-area);
  }

  void backward() {}
};

} // namespace NN
} // namespace PS
