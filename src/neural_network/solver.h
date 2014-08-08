#pragma once
#include "system/app.h"
#include "neural_network/net.h"

namespace PS {
namespace NN {

class Solver : public App {
 public:
  void init();

 protected:
  typedef double real;
  unique_ptr<Net<real>> train_;
  unique_ptr<Net<real>> test_;

};

void Solver::init() {
  CHECK(app_cf_.has_nn_train());
  train_.reset(new Net<real>(app_cf_.nn_train()));
  train_->init();
}

} // namespace NN
} // namespace PS
