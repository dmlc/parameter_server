#pragma once
#include "system/app.h"
#include "neural_network/net.h"

namespace PS {
namespace NN {

class Solver : public App {
 public:
  void init();
 protected:
  unique_ptr<Net<float>> train_;

};

void Solver::init() {
  CHECK(app_cf_.has_nn());
  train_.reset(new Net<float>(app_cf_.nn()));
}

} // namespace NN
} // namespace PS
