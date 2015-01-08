#pragma once
#include "learner/sgd_comp_node.h"
namespace PS {

class SGDServer : public SGDCompNode {
 public:
  SGDServer(const string& name) : SGDCompNode(name) { }
  virtual ~SGDServer() { }

  virtual void process(const MessagePtr& msg) {
    auto sgd = get(msg);
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      startReporter(sgd.report_interval());
      updateModel();
    } else if (sgd.cmd() == SGDCall::SAVE_MODEL) {
      saveModel();
    }
  }

  virtual void evaluateProgress(SGDProgress* prog) {
    Lock l(progress_mu_);
    *prog = progress_;
    progress_.Clear();
  }

  virtual void updateModel() = 0;
  virtual void saveModel() = 0;
};

} // namespace PS
