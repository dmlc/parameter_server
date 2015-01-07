#pragma once
#include "parameter/kv_store.h"
namespace PS {

template<typename V>
struct AdaGradEntry {
  AdaGradEntry() { weight = 0; sum_sqr_grad = 1e-20; }
  V weight;
  V sum_sqr_grad;
  static V eta = .1;
  static V lambda = 1;
  void get(char const* data) {
    V grad = *((V*)data);
    sum_sqr_grad += grad * grad;
    V delta = eta * (grad / sqrt(sum_sqr_grad) + lambda * weight);
    weight -= delta > 1.0 ? 1.0 : ( delta < -1.0 ? -1.0 : delta );
  }
  void set(char* data) {
    *((V*)data) = weight;
  }
};

template<typename V>
class AdaGradUpdater : public KVStore<Key, AdaGradEntry<V>> {
public:

};


template<typename V>
using AdaGradUpdaterPtr = std::shared_ptr<AdaGradUpdater<V>>;

} // namespace PS
