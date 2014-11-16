#pragma once
#include "linear_method/computation_node.h"
#include "linear_method/batch_common.h"
namespace PS {
namespace LM {

class BatchServer : public CompNode, public BatchCommon {
 public:
  virtual void init();
  virtual void preprocessData(const MessagePtr& msg);
  virtual void iterate(const MessagePtr& msg) { updateWeight(msg); }
  // TODO
  virtual void evaluateProgress(Progress* prog) { }

  virtual void saveModel() {
    if (conf_.has_model_output()) {
      saveModel(conf_.model_output());
    }
  }
 protected:
  // TODO
  void updateWeight(const MessagePtr& msg) { }
  void saveModel(const DataConfig& output);

};





} // namespace LM
} // namespace PS
