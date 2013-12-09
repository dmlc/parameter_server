#pragma once

#include "algo/inference.h"
#include "util/eigen3.h"

namespace PS {

// distributed gradient descent sovles logistic regression
class GD : public Inference {
 public:
  GD(string name) : Inference(name) { }
  void Client();
  void Server();
 private:
  // training data
  DSMat X_;
  DVec Y_;
};
}
