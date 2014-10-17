#pragma once
#include "proto/config.pb.h"

namespace PS {

template<typename V>
class LearningRate {
 public:
  LearningRate() { }
  LearningRate(const LearningRateConfig& cf) { set(cf); }

  void set(const LearningRateConfig& cf) { cf_ = cf; }

  V evaluate(int t) {
    typedef LearningRateConfig Type;
    if (cf_.type() == Type:CONSTANT) {
      return (V)cf_.eta();
    }
    return 1.0;
  }

 private:
  LearningRateConfig cf_;
};

} // namespace PS
