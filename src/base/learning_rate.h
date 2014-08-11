#pragma once
#include "proto/config.pb.h"

namespace PS {

class LearningRate {
 public:
  LearningRate(const LearningRateConfig& cf) : cf_(cf) { }

  void set(const LearningRateConfig& cf) { cf_ = cf; }

  double evaluate(int t) {
    typedef LearningRateConfig Type;
    if (cf_.type() == Type:CONSTANT) {
      return cf_.eta();
    }
    return 1.0;
  }

 private:
  LearningRateConfig cf_;
};

} // namespace PS
