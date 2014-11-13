#pragma once
#include "linear_method/ftrl.h"
#include "parameter/kv_vector.h"
#include "util/producer_consumer.h"
#include "base/localizer.h"
#include "linear_method/loss_inl.h"
namespace PS {
namespace LM {

class FTRLWorker {
 public:
  void init(const string& name, const Config& conf);
  void computeGradient();
  void evaluateProgress(Progress* prog);
 private:
  Config conf_;
  LossPtr<real> loss_;
  KVVectorPtr<Key, real> model_;

  struct Minibatch {
    MatrixPtr<real> label;
    LocalizerPtr<Key, real> localizer;
    int batch_id;
    int pull_time;
  };
  ProducerConsumer<Minibatch> data_prefetcher_;

  struct Status {
    uint64 num_ex = 0;
    real objv = 0;
    real acc = 0;
    void reset() { num_ex = 0; objv = 0; acc = 0; }
  };
  Status status_;
  std::mutex status_mu_;
};

} // namespace LM
} // namespace PS

// void countFrequency(const MatrixPtr<real>& Y, const MatrixPtr<real>& X,
//                     SArray<uint32>* pos, SArray<uint32>* neg);
