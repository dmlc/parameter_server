#pragma once
#include "learner/sgd.h"
#include "data/stream_reader.h"
#include "util/evaluation.h"
#include "parameter/kv_vector.h"
#include "parameter/kv_store.h"
#include "linear_method/learning_rate.h"
namespace PS {
namespace LM {

// project value into [-bound, bound]
template <typename V>
V project(V value, V bound) {
  return (value > bound ? bound : (value < - bound ? - bound : value));
}

// async sgd. support various loss functions and regularizers
class AsyncSGDScheduler : public ISGDScheduler, public LinearMethod {
 public:
  AsyncSGDScheduler(const string& name, const Config& conf)
      : ISGDScheduler(name), LinearMethod(conf) { }
  virtual ~AsyncSGDScheduler() { }

  virtual void run() {
    updateModel(conf_.training_data());
    saveModel();
  }
};

template <typename V>
struct SGDState {
  SGDState() { }
  SGDState(const PenaltyConfig& h_conf, const LearningRateConfig& lr_conf) {
    lr = shared_ptr<LearningRate<V>>(new LearningRate<V>(lr_conf));
    h = shared_ptr<Penalty<V>>(createPenalty<V>(h_conf));
  }
  virtual ~SGDState() { }

  void update(V new_weight, V old_weight) {
    if (new_weight == 0 && old_weight != 0) {
      -- nnz;
    } else if (new_weight != 0 && old_weight == 0) {
      ++ nnz;
    }
  }

  shared_ptr<LearningRate<V>> lr;
  shared_ptr<Penalty<V>> h;

  int iter = 0;
  size_t nnz = 0;
  V max_delta = 1.0;  // maximal change of weight
};

template <typename V>
struct AdaGradEntry {
  void get(char const* data, SGDState<V>* state) {
    // update model
    V grad = *((V*)data);
    sum_sq_grad += grad * grad;
    // state->update(weight, grad, sqrt(sum_sq_grad));
    // TODO
  }

  void put(char* data, SGDState<V>* state) {
    *((V*)data) = weight;
  }
  V weight = 0;
  V sum_sq_grad = 0;
};

template <typename V>
struct SGDEntry {
  void get(char const* data, SGDState<V>* state) {
    V grad = *((V*)data);
    // state->update(weight, grad);
    // TODO
  }
  void put(char* data, SGDState<V>* state) {
    *((V*)data) = weight;
  }
  V weight = 0;
};

template <typename V>
struct FTRLEntry {
  V w = 0;  // not necessary to store w, because it can be computed from z
  V z = 0;
  V sqrt_n = 0;

  void get(char const* data, SGDState<V>* state) {
    // update model
    V w_old = w;
    V grad = *((V*)data);
    V sqrt_n_new = sqrt(sqrt_n * sqrt_n + grad * grad);
    V sigma = (sqrt_n_new - sqrt_n) / state->lr->alpha();

    z += grad  - sigma * w;
    sqrt_n = sqrt_n_new;

    V eta = state->lr->eval(sqrt_n);
    w = state->h->proximal(-z*eta, eta);
    state->update(w, w_old);
  }

  void put(char* data, SGDState<V>* state) {
    *((V*)data) = w;
  }
};



template <typename V>
class AsyncSGDServer : public ISGDServer, public LinearMethod {
public:
  AsyncSGDServer(const string& name, const Config& conf)
      : ISGDServer(name), LinearMethod(conf) {
    if (conf_.async_sgd().algo() == SGDConfig::FTRL) {
      model_ = CHECK_NOTNULL((
          new KVStore<Key, FTRLEntry<V>, SGDState<V>>(name+"_model", name)));
    } else {
      if (conf_.async_sgd().ada_grad()) {
        model_ = CHECK_NOTNULL((
            new KVStore<Key, SGDEntry<V>, SGDState<V>>(name+"_model", name)));
      } else {
        model_ = CHECK_NOTNULL((
            new KVStore<Key, AdaGradEntry<V>, SGDState<V>>(name+"_model", name)));
      }
    }
  }

  virtual ~AsyncSGDServer() {
    delete model_;
  }

  void init() {
    model_->setEntrySyncSize(sizeof(V));

    SGDState<V> state(conf_.penalty(), conf_.learning_rate());
    model_->setState(state);

    // tail feature filter
    model_->setTailFilterSize(
        0, conf_.async_sgd().countmin_n()/sys_.yp().num_servers(),
        conf_.async_sgd().countmin_k());
  }

  void evaluate(SGDProgress* prog) {
    prog->set_nnz(model_->state().nnz);
  }

  void saveModel() {
    // TODO
  }
 protected:
  KVState<Key, SGDState<V>>* model_;
};

template <typename V>
using AsyncSGDWorkerBase = ISGDWorker<StreamReader<V>, SparseMinibatch<V>>;

template <typename V>
class AsyncSGDWorker : public AsyncSGDWorkerBase<V>, public LinearMethod {
 public:
  AsyncSGDWorker(const string& name, const Config& conf)
      : AsyncSGDWorkerBase<V>(name), LinearMethod(conf), model_(name+"_model", name) {
    loss_ = createLoss<V>(conf_.loss());
  }
  virtual ~AsyncSGDWorker() { }

  virtual bool readMinibatch(StreamReader<V>& reader, SparseMinibatch<V>* data) {
    // read a minibatch
    MatrixPtrList<V> ins;
    const auto& sgd = conf_.async_sgd();
    if (!reader.readMatrices(sgd.minibatch(), &ins)) return false;
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
    tail->set_query_key(sgd.tail_feature_freq());
    tail->set_query_value(true);
    data->pull_time = model_.pull(msg);
    return true;
  }

  virtual void computeGradient(SparseMinibatch<V>& data) {
    // release some memory
    int id = data.batch_id;
    if (pre_batch_ >= 0) model_.clear(pre_batch_);
    pre_batch_ = id;
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
    // not with penalty.
    // + penalty_->evaluate(w.matrix());
    V auc = Evaluation<V>::auc(Y->value(), Xw);
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

} // namespace LM
} // namespace PS
