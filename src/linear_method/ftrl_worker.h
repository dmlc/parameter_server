#pragma once
#include "linear_method/ftrl.h"
#include "parameter/kv_vector.h"
#include "util/producer_consumer.h"

namespace PS {
namespace LM {

class FTRLWorker {
 public:
  void init(const Config& conf);
  void computeGradient();
  void evaluateProgress(Progress* prog);
 private:
  Config conf_;
  LossPtr<Real> loss_;
  KVVector<Key, Real> model_;

  struct Minibatch {
    MatrixPtr<Real> label;
    Localizer<Key, Real> localizer;
    int batch_id;
    int pull_time;
  };
  ProducerConsumer<Minibatch> data_prefetcher_;

  struct Status {
    uint64 num_ex = 0;
    Real objv = 0;
    Real acc = 0;
    void reset() { num_ex = 0; objv = 0; acc = 0; }
  };
  Status status_;
  std::mutex status_mu_;
};

} // namespace LM
} // namespace PS

// void countFrequency(const MatrixPtr<Real>& Y, const MatrixPtr<Real>& X,
//                     SArray<uint32>* pos, SArray<uint32>* neg);
