#pragma once
#include "linear_method/linear_method.h"
namespace PS {
namespace LM {

// the base class for a worker or a server
class CompNode : public LinearMethod {
 public:
  virtual void init() { LinearMethod::init(); }
  void run() { }

 protected:
  virtual void loadData(ExampleInfo* info, int* hit_cache) { }
  virtual void preprocessData(const MessagePtr& msg) { }
  virtual void iterate(const MessagePtr& msg) { }
  virtual void evaluateProgress(Progress* prog) { }
  virtual void saveModel() { }

};
} // namespace LM
} // namespace PS
