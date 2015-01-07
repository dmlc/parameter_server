#pragma once
#include "learner/sgd_server.h"
#include "learner/sgd_model.h"
#include "factorization_machine/fm.h"
namespace PS {
namespace FM {

class FMServer : public SGDServer {
 public:
  virtual void init() {
    // w_ = AdaGradUpdaterPtr<Real>>(new AdaGradUpdater<Real>());
    // REGISTER_CHILD_CUSTOMER(name() + "_w", w_, name());
  }
  virtual void updateModel() { }
  virtual void saveModel() { }
 protected:
  // AdaGradUpdaterPtr<Real> w_;
};
} // namespace LM
} // namespace PS
