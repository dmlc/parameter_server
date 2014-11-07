#pragma once
#include "linear_method/linear_method.h"

namespace PS {
namespace LM {

class OnlineSolver : public LinearMethod {
 public:
  void init() { LinearMethod::init(); }
};

} // namespace LM
} // namespace PS
