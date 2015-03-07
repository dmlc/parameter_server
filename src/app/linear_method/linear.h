#pragma once
#include "app/linear_method/proto/linear.pb.h"
#include "app/linear_method/loss.h"
#include "app/linear_method/penalty.h"
namespace PS {
namespace LM {

class LinearMethod {
 public:
  LinearMethod(const Config& conf) : conf_(conf) { }
  virtual ~LinearMethod() { }
 protected:
  Config conf_;
};

} // namespace LM
} // namespace PS
