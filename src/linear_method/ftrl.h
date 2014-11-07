#pragma once
#include "parameter/shared_parameter.h"
#include "parameter/kv_vector.h"
#include "linear_method/online_solver.h"
#include "linear_method/ftrl_model.h"
#include "linear_method/loss_inl.h"
#include "util/threadsafe_limited_queue.h"
namespace PS {
namespace LM {

// Online gradient descent optimized for l_1-regularized logistic regression. It
// has been used by Google's Smartass, more details are in the paper
//
// H. McMahan. et.al, Ad Click Prediction: a View from the Trenches, KDD'13
//
class FTRL : public OnlineSolver {
 public:
  void init();
  void run();

  void saveModel(const MessageCPtr& msg);
  void updateModel(const MessagePtr& msg);

 protected:
  typedef double Real;
  void countKeys(const MatrixPtr<Real>& Y, const MatrixPtr<Real>& X,
                 SArray<uint32>* pos, SArray<uint32>* neg);
  void evalProgress();
  void showProgress();

  KVVectorPtr<Key, Real> worker_w_;
  FTRLModelPtr<Key, Real> server_w_;

  LossPtr<Real> loss_;

  struct Status {
    uint64 num_ex = 0;
    Real objv = 0;
    Real acc = 0;
    void reset() { num_ex = 0; objv = 0; acc = 0; }
  };
  Status status_;
  std::mutex status_mu_;
  unique_ptr<std::thread> prog_thr_;

  // read minibatches
  unique_ptr<std::thread> data_thr_;
  bool read_data_finished_ = false;
  threadsafeLimitedQueue<MatrixPtrList<Real> > data_buf_;
};

} // namespace LM
} // namespace PS
