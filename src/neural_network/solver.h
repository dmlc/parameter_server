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
  NetPtr<real> train_;
  NetPtr<real> test_;

  SolverConfig cf_;
};

void Solver::init() {
  CHECK(app_cf_.has_neural_network());

  // TODO
  // train_.reset(new Net<real>(app_cf_.nn_train()));
  // train_->init();

  // if (app_cf_.has_nn_test()) {
  //   test_.reset(new Net<real>(app_cf_.nn_test()));
  //   test_->init(train_);
  // }
  // CHECK(app_cf_.has_nn_solver());
  // cf_ = app_cf_.nn_solver();
}

} // namespace NN
} // namespace PS
