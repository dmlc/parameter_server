#pragma once
#include "util/common.h"
#include "proto/config.pb.h"

namespace PS {

// the base virtual class of a learning algorithm, for any new algorithm, add an
// according entry in the factory function _create_
template<typename T>
class Learner {
 public:
  // virtual void init() { }
  void set(const LearnerConfig& config) { learner_cf_ = config; }

 protected:
  LearnerConfig learner_cf_;
};

template<typename T> using LearnerPtr = std::shared_ptr<Learner<T> >;

} // namespace PS
