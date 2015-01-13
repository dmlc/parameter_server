#pragma once
#include "learner/sgd.h"
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
class FTRLServer : public SGDServer, LinearMethod {
public:
  FTRLServer(const string& name, const Config& conf)
      : SGDServer(name), LinearMethod(conf), model_(name+"_model", name) { }
  virtual ~FTRLServer() { }

  typedef FTRLEntry<V> E;
  void init() {
    model_.setEntrySyncSize(sizeof(V));

    FTRLState<V> state;
    auto ftrl = conf_.ftrl();
    // set learning rate and panlty
    state.alpha = ftrl.learning_rate().alpha();
    state.beta = ftrl.learning_rate().beta();

    auto rg = conf_.penalty();
    if (rg.lambda_size() > 0) state.lambda1 = rg.lambda(0);
    if (rg.lambda_size() > 1) state.lambda2 = rg.lambda(1);
    model_.setState(state);

    // tail feature filter
    model_.setTailFilterSize(
        0, ftrl.countmin_n()/sys_.yp().num_servers(), ftrl.countmin_k());
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
using FTRLWorkerBase = SGDWorker<StreamReader<V>, SparseMinibatch<V>>;

template <typename V>
class FTRLWorker : public FTRLWorkerBase<V>, LinearMethod {
 public:
  FTRLWorker(const string& name, const Config& conf)
      : FTRLWorkerBase<V>(name), LinearMethod(conf), model_(name+"_model", name) {
    loss_ = createLoss<V>(conf_.loss());
  }
  virtual ~FTRLWorker() { }
  bool readMinibatch(StreamReader<V>& reader, SparseMinibatch<V>* data) {
    // read a minibatch
    MatrixPtrList<V> ins;
    const auto& ftrl = conf_.ftrl();
    if (!reader.readMatrices(ftrl.minibatch(), &ins)) return false;
    CHECK_EQ(ins.size(), 2);
    // LL << ins[0]->debugString() << "\n" << ins[1]->debugString();
    data->label = ins[0];
    data->batch_id = batch_id_ ++;

    // find all unique features,
    SArray<Key> uniq_key;
    SArray<uint8> key_cnt;
    data->localizer = LocalizerPtr<Key, V>(new Localizer<Key, V>());
    data->localizer->countUniqIndex(ins[1], &uniq_key, &key_cnt);

    // pull the features and weights from servers with tails filtered
    MessagePtr msg(new Message(kServerGroup));
    msg->task.set_key_channel(data->batch_id);
    msg->setKey(uniq_key);
    msg->addValue(key_cnt);
    msg->addFilter(FilterConfig::KEY_CACHING);
    auto tail = SharedParameter<Key>::set(msg)->mutable_tail_filter();
    tail->set_insert_count(true);
    tail->set_query_key(ftrl.tail_feature_freq());
    tail->set_query_value(true);
    data->pull_time = model_.pull(msg);
    return true;
  }
  void computeGradient(SparseMinibatch<V>& data) {
    // release some memory
    int id = data.batch_id;
    if (pre_batch_ >= 0) {
      model_.clear(pre_batch_);
      pre_batch_ = id;
    }
    // waiting the model working set
    model_.waitOutMsg(kServerGroup, data.pull_time);

    // localize the feature matrix
    auto X = data.localizer->remapIndex(model_.key(id));
    auto Y = data.label;
    CHECK_EQ(X->rows(), Y->rows());

    // compute the gradient
    SArray<V> Xw(Y->rows());
    auto w = model_.value(id);
    Xw.eigenArray() = *X * w.eigenArray();
    V objv = loss_->evaluate({Y, Xw.matrix()});
    V auc = Evaluation<V>::auc(Y->value(), Xw);
    // not with penalty.
    // penalty_->evaluate(w.matrix());
    {
      Lock l(this->progress_mu_);
      auto& p = this->progress_;
      p.add_objective(objv);
      p.add_auc(auc);
      p.set_num_examples_processed(
          p.num_examples_processed() + Xw.size());
    }
    SArray<V> grad(X->cols());
    loss_->compute({Y, X, Xw.matrix()}, {grad.matrix()});

    // push the gradient
    MessagePtr msg(new Message(kServerGroup));
    msg->setKey(model_.key(data.batch_id));
    msg->addValue(grad);
    msg->task.set_key_channel(id);
    msg->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    model_.push(msg);
  }
private:
  KVVector<Key, V> model_;
  LossPtr<V> loss_;
  int batch_id_ = 0;
  int pre_batch_ = -1;

};

class FTRLScheduler : public SGDScheduler, LinearMethod {
 public:
  FTRLScheduler(const string& name, const Config& conf)
      : SGDScheduler(name), LinearMethod(conf) { }
  virtual ~FTRLScheduler() { }

  virtual void run() {
    updateModel(conf_.training_data());
    saveModel();
  }
};

} // namespace LM
} // namespace PS
