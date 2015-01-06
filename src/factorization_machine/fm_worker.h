#pragma once
#include "factorization_machine/fm.h"
#include "learner/sgd_worker.h"
namespace PS {
namespace FM {

class FMWorker : public SGDWorker<double> {
 public:
  virtual void init() { }
  virtual bool readMinibatch(StreamReader<double>& reader, Minibatch* data) {  }
  virtual void computeGradient(Minibatch& data) { }
};
} // namespace FM
} // namespace PS
