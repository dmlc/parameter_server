#pragma once
#include "learner/learner.h"
#include "learner/gradient_descent.h"
#include "learner/proximal_gradient.h"

namespace PS {

template<typename T>
class LearnerFactory {
 public:
  static LearnerPtr<T> create(const LearnerConfig& config) {
    typedef LearnerConfig Config;
    CHECK(config.has_type());
    LearnerPtr<T> ptr;
    switch(config.type()) {
      case Config::GRADIENT_DESCENT:
        ptr = LearnerPtr<T>(new GradientDescent<T>());
        break;
      case Config::PROXIMAL_GRADIENT:
        ptr = LearnerPtr<T>(new ProximalGradient <T>());
        break;
      default:
        CHECK(false) << "unknown type: " << config.DebugString();
    }
    ptr->set(config);
    // ptr->init();
    return ptr;
  }
};

} // namespace PS
