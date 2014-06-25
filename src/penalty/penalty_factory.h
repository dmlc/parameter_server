#pragma once
#include "penalty/penalty.h"
#include "penalty/p_norm_penalty.h"
#include "proto/config.pb.h"

namespace PS {

template<typename T>
class PenaltyFactory {
 public:
  static PenaltyPtr<T> create(const PenaltyConfig& config) {
    switch (config.type()) {
      case PenaltyConfig::L1:
        return PenaltyPtr<T>(new PNormPenalty<T>(1, config.coefficient()));
      case PenaltyConfig::L2:
        return PenaltyPtr<T>(new PNormPenalty<T>(2, config.coefficient()));
      default:
        CHECK(false) << "unknown type: " << config.DebugString();
    }
    return PenaltyPtr<T>(nullptr);
  }
};

} // namespace PS
