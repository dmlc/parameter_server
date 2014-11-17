#pragma once
#include "linear_method/computation_node.h"
#include "linear_method/ftrl_common.h"
#include "parameter/kv_vector.h"
#include "util/producer_consumer.h"
#include "base/localizer.h"
#include "linear_method/loss_inl.h"
#include "linear_method/progress_reporter.h"
namespace PS {
namespace LM {

class FTRLWorker : public CompNode {
 public:
  virtual void init();
  virtual void iterate(const MessagePtr& msg) {
    report.start(this, conf_.solver().eval_interval());
    computeGradient();
  }
  virtual void evaluateProgress(Progress* prog);
 private:
  void computeGradient();
  KVVectorPtr<Key, real> model_;

  struct Minibatch {
    MatrixPtr<real> label;
    LocalizerPtr<Key, real> localizer;
    int batch_id;
    int pull_time;
  };
  ProducerConsumer<Minibatch> data_prefetcher_;

  Progress prog_;
  std::mutex prog_mu_;


  // struct Status {
  //   uint64 num_ex = 0;
  //   real objv = 0;
  //   real acc = 0;
  //   real auc = 0;
  //   // void reset() { num_ex = 0; objv = 0; acc = 0; }
  // };
  // std::vector<Status> status_;

  ProgressReporter report;
};

} // namespace LM
} // namespace PS

// void countFrequency(const MatrixPtr<real>& Y, const MatrixPtr<real>& X,
//                     SArray<uint32>* pos, SArray<uint32>* neg);
