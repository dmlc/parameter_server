#pragma once
#include "loss/loss.h"
#include "proto/config.pb.h"
#include "loss/logit_loss.h"

namespace PS {

template<typename T>
class LossFactory {
 public:
  static LossPtr<T> create(const LossConfig& config) {
    switch (config.type()) {
      case LossConfig::LOGIT:
        return LossPtr<T>(new LogitLoss<T>());
      default:
        CHECK(false) << "unknown type: " << config.DebugString();
    }
    return LossPtr<T>(nullptr);
  }
};

} // namespace PS
