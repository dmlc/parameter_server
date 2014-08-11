#pragma once
#include "neural_network/solver.h"

namespace PS {
namespace NN {

class SGDSolver : public Solver {
 public:
  void run();

  void update(int iter, ParameterPtr<real>& param) {
    if (!param || !(param->value) || !(param->gradient)) return;
    // LL << param->value->eigenArray()[0]
    //    << " " << param->gradient->eigenArray()[0];
    param->value->eigenArray() -= cf_.lr().eta() * param->gradient->eigenArray();
  }

  void process(Message* msg) { }

};

void SGDSolver::run() {

  int iter = 0;
  for (; iter < cf_.max_iteration(); ++iter) {
    real objv = 0;
    for (auto& l : train_->layers()) {
      objv += l->forward();
    }
    for (int i = train_->layers().size(); i > 0 ; --i) {
      auto& l = train_->layers()[i-1];
      l->backward();
      update(iter, l->model());
    }
    LL << iter << " objv: " << objv;

    real auc = 0;
    for (auto& l : test_->layers()) {
      auc += l->forward();
    }
    LL << "test auc: " << auc;
  }
  LL << "finished";
}

} // namespace NN
} // namespace PS
