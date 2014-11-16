#pragma once
#include "linear_method/batch_server.h"
#include "linear_method/darlin_common.h"
namespace PS {
namespace LM {

class DarlinServer : public BatchServer, public DarlinCommon {
 public:
  virtual void preprocessData(const MessagePtr& msg);
  virtual vodi iterate(const MessagePtr& msg) { updateWeight(msg); }
  virtual void evaluateProgress(Progress* prog);
 protected:
  void updateWeight(const MessagePtr& msg);
  double newDelta(double delta_w) {
    return std::min(conf_.darling().delta_max_value(), 2 * fabs(delta_w) + .1);
  }
  double kkt_filter_threshold_;
  double violation_;
};

} // namespace LM
} // namespace PS
