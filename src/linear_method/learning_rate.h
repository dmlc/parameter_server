#pragma once
#include "linear_method/proto/lm.pb.h"
namespace PS {
namespace LM {

template <typename V>
class LearningRate {
 public:
  LearningRate(const LearningRateConfig& conf) : conf_(conf) {
    CHECK_GE(conf.coef_size(), 1);
    for (int i = 0; i < conf.coef_size(); ++i) {
      CHECK_GT(conf.coef(i), 0);
    }
  }
  ~LearningRate() { }

  V eval(V x = 0) const {
    if (conf_.coef_size() == 1) {
      return alpha();
    } else if (conf_.coef_size() == 2) {
      CHECK_GE(x, 0);
      return alpha() / ( x + beta() );
    } else {
      CHECK(false);
    }
  }

  V alpha() const { return conf_.coef(0); }
  V beta() const { return conf_.coef(1); }
 private:
  // const LearningRateConfig& conf() { return conf_; }
  LearningRateConfig conf_;
};


} // namespace LM
} // namespace PS
