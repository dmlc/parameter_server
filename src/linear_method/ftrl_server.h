#pragma once
#include "linear_method/computation_node.h"
#include "linear_method/ftrl_common.h"
#include "linear_method/ftrl_model.h"
#include "linear_method/progress_reporter.h"
namespace PS {
namespace LM {

class FTRLServer : public CompNode {
 public:
  virtual void init() {
    CompNode::init();
    model_ = std::shared_ptr<FTRLModel>(new FTRLModel());
    model_->init(conf_);
    REGISTER_CUSTOMER(app_cf_.parameter_name(0), model_);
  }

  virtual void iterate(const MessagePtr& msg) {
    report.start(this, conf_.solver().eval_interval());
  }

  virtual void evaluateProgress(Progress* prog) {
    model_->evaluateProgress(prog);
  }

  virtual void saveModel() {
    if (conf_.has_model_output()) {
      auto out = ithFile(conf_.model_output(), 0, "_" + myNodeID());
      model_->writeToFile(out);
    }
  }
 protected:
  std::shared_ptr<FTRLModel> model_;
  ProgressReporter report;
};

} // namespace LM
} // namespace PS
