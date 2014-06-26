#pragma once
#include "app/linear_block_iterator.h"

namespace PS {

// optimizated for sparse logisitic regression
class BlockCoordinateL1LR : public LinearBlockIterator {
 public:
  virtual void run() { LinearBlockIterator::run(); }
 protected:
  virtual void prepareData(const Message& msg) {
    LinearBlockIterator::prepareData(msg);
    if (exec_.isWorker()) {
      // dual_ = exp(X_*w_)
      dual_.array() = exp(dual_.array());
    }
  }
  virtual void updateModel(Message* msg) {

  }
};

} // namespace PS
