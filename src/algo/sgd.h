#pragma once

#include "algo/inference.h"
#include "util/eigen3.h"

namespace PS {

// distributed stochastic gradient descent sovles
//      min_w 1/2 ||y-Xw||^2 + lambdae ||w||^2
class SGD : public Inference {
 public:
  SGD(string name) : Inference(name) { }
  void Client();
  void Server();
 private:
  // training data
  DSMat X_;
  DVec Y_;
};
}
