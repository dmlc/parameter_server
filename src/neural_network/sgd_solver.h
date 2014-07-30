#pragma once
#include "neural_network/solver.h"

namespace PS {
namespace NN {

class SGDSolver : public Solver {
 public:
  void run();

  void process(Message* msg) { }
};

void SGDSolver::run() {
  CHECK(app_cf_.has_nn_solver());
  SolverConfig cf = app_cf_.nn_solver();

  int iter = 0;
  for (; iter < cf.max_iteration(); ++iter) {
    for (auto& l : net_->layers()) l->forward();
    for (auto& l : net_->layers()) l->backward();
    for (auto& l : net_->layers()) l->update();
  }

  LL << "finished";
}

} // namespace NN
} // namespace PS
