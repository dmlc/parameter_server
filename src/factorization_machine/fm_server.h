#pragma once
#include "learner/sgd_server.h"
namespace PS {
namespace FM {

class FMServer : public SGDServer {
 public:
  virtual void init() { }
  virtual void updateModel() { }
  virtual void saveModel() { }
};
} // namespace LM
} // namespace PS
