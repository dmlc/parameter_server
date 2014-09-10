#pragma once
#include "neural_network/solver.h"

namespace PS {
namespace NN {

class SGDSolver : public Solver {
 public:
  void run();

  void update(int iter, ParameterPtr<real>& param) {
    if (!param || !(param->value) || !(param->gradient)) return;

    real alpha = cf_.lr().alpha();
    real beta = cf_.lr().beta();
    auto grad = param->gradient->eigenArray();
    // LL << grad.matrix().squaredNorm();

    if (cf_.update() == SolverConfig::ADAGRAD) {
      const auto& name = param->name;
      if (past_grads_.count(name) == 0) {
        past_grads_[name] = MatrixPtr<real>(
            new DenseMatrix<real>(
                param->gradient->rows(), param->gradient->cols()));
      }

      auto ada_grad = past_grads_[name]->eigenArray();

      ada_grad += grad.square();

      LL << ada_grad.matrix().squaredNorm();
      param->value->eigenArray() -=  alpha * grad / (beta + ada_grad.sqrt());
    } else {
      param->value->eigenArray() -=  alpha * grad / (beta + sqrt((real)iter));
    }
  }

  void process(const MessagePtr& msg) { }
 protected:
  std::map<string, MatrixPtr<real>> past_grads_;
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

    if ((iter+1) % cf_.validation() == 0) {
      real auc = 0;
      for (auto& l : test_->layers()) {
        auc += l->forward();
      }
      LL << "test auc: " << auc;
    }
  }
  LL << "finished";
}

} // namespace NN
} // namespace PS
