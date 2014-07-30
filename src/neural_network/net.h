#pragma once

#include "proto/neural_network.pb.h"

namespace PS {
namespace NN {

template<typename V> class Net {
 public:
  explicit Net(const NetConfig& config)
      : cf_(config) { }

  // construct the network
  void init();


 protected:
  NetConfig cf_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Net);
};

} // namespace NN
} // namespace PS
