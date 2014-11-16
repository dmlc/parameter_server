#pragma once
#include "linear_method/linear_method.h"

namespace PS {
namespace LM {

class OnlineSolver : public LinearMethod {
 public:
  void init() { LinearMethod::init(); }
  virtual void process(const MessagePtr& msg) { }
};

} // namespace LM
} // namespace PS
