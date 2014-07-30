#pragma once
#include "system/app.h"
#include "neural_network/net.h"

namespace PS {
namespace NN {

class Solver : public App {
 public:
  void init();
 protected:
  unique_ptr<Net<float>> net_;

};

void Solver::init() {
  CHECK(app_cf_.has_nn());
  net_->init(app_cf_.nn());

}

} // namespace NN
} // namespace PS
