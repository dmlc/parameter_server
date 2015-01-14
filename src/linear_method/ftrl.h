#pragma once
#include "linear_method/async_sgd.h"
#include "data/stream_reader.h"
#include "base/evaluation.h"
#include "parameter/kv_vector.h"
#include "parameter/kv_store.h"
namespace PS {
namespace LM {

template <typename V>
V softThresholding(V x, V lambda_1, V lambda_2) {
  if (x > 0) {
    return x > lambda_1 ? (x - lambda_1) / lambda_2 : 0;
  } else {
    return x < - lambda_1 ? (x + lambda_1) / lambda_2 : 0;
  }
}

template <typename V>
struct FTRLState {
  // learning rate
  V alpha, beta;
  // penalty
  V lambda1 = 0, lambda2 = 0;
  // progress
  V norm1 = 0, norm2 = 0;
  size_t nnz = 0;
};

template <typename V>
struct FTRLEntry {
  V w = 0;  // not necessary to store w, because it can be computed from z
  V z = 0;
  V sqrt_n = 0;

  void get(char const* data, FTRLState<V>* state) {
    // update model
    V w_old = w;
    V grad = *((V*)data);
    V sqrt_n_new = sqrt(sqrt_n * sqrt_n + grad * grad);
    V sigma = (sqrt_n_new - sqrt_n) / state->alpha;
    z += grad  - sigma * w;
    sqrt_n = sqrt_n_new;
    V lambda2 = state->lambda2 + (state->beta + sqrt_n_new) / state->alpha;
    w = - softThresholding(z, state->lambda1, lambda2);

    // update status
    state->norm1 += fabs(w) - fabs(w_old);
    state->norm2 += w * w - w_old * w_old;
    if (w == 0 && w_old != 0) {
      -- state->nnz;
    } else if (w != 0 && w_old == 0) {
      ++ state->nnz;
    }
  }

  void put(char* data, FTRLState<V>* state) {
    *((V*)data) = w;
  }
};


template <typename V>
class FTRLServer : public ISGDServer, LinearMethod {
public:
  FTRLServer(const string& name, const Config& conf)
      : ISGDServer(name), LinearMethod(conf), model_(name+"_model", name) { }
  virtual ~FTRLServer() { }

  void init() {
    model_.setEntrySyncSize(sizeof(V));

    FTRLState<V> state;
    auto sgd = conf_.sgd();
    // set learning rate and panlty
    state.alpha = sgd.learning_rate().alpha();
    state.beta = sgd.learning_rate().beta();

    auto rg = conf_.penalty();
    if (rg.lambda_size() > 0) state.lambda1 = rg.lambda(0);
    if (rg.lambda_size() > 1) state.lambda2 = rg.lambda(1);
    model_.setState(state);

    // tail feature filter
    model_.setTailFilterSize(
        0, sgd.countmin_n()/sys_.yp().num_servers(), sgd.countmin_k());
  }

  void evaluate(SGDProgress* prog) {
    auto s = model_.state();
    prog->set_nnz(s.nnz);
    // prog->add_objective(s.norm1 * s.lambda1 + .5 * s.lambda2 * sqrt(s.norm2));
  }

  void saveModel() {
    // TODO
  }
 protected:
  KVStore<Key, FTRLEntry<V>, FTRLState<V>> model_;
};

template <typename V>
class FTRLWorker : public AsyncSGDWorker<V> {
 public:
  FTRLWorker(const string& name, const Config& conf)
      : AsyncSGDWorker<V>(name, conf) { }
  virtual ~FTRLWorker() { }
};

class FTRLScheduler : public AsyncSGDScheduler {
 public:
  FTRLScheduler(const string& name, const Config& conf)
      : AsyncSGDScheduler(name, conf) { }
  virtual ~FTRLScheduler() { }
};

} // namespace LM
} // namespace PS
