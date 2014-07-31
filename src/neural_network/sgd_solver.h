#pragma once
#include "neural_network/solver.h"

namespace PS {
namespace NN {

class SGDSolver : public Solver {
 public:
  void run();

  void updater(int iter, ParameterPtr<float>& param) {
    param->value->eigenArray() -= .1 * param->gradient->eigenArray();
  }

  void process(Message* msg) { }

};

void SGDSolver::run() {
  CHECK(app_cf_.has_nn_solver());
  SolverConfig cf = app_cf_.nn_solver();

  int iter = 0;
  for (; iter < cf.max_iteration(); ++iter) {
    float objv = 0;
    for (auto& l : train_->layers()) {
      objv += l->forward();
    }
    for (auto& l : train_->layers()) {
      l->backward();
    }

    for (auto& l : train_->layers()) {
      updater(iter, l->model());
    }

    LL << iter << " objv: " << objv;
  }

  LL << "finished";
}

} // namespace NN
} // namespace PS
