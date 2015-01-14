#pragma once
#include "learner/sgd.h"
#include "data/stream_reader.h"
#include "base/evaluation.h"
#include "parameter/kv_vector.h"
#include "parameter/kv_store.h"
namespace PS {
namespace LM {

// project value into [-bound, bound]
template <typename V>
V project(V value, V bound) {
  return (value > bound ? bound : (value < - bound ? - bound : value));
}

// async sgd. support various loss functions and l_2
// regularizer. check ftrl.h for l_1 regularizer
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
  int iter = 0;
  LearningRateConfig lr;
  V lambda = 0;
  V penalty = 0;
  V max_delta = 1.0;  // maximal change of weight
};

template <typename V>
struct AdaGradEntry {
  V weight = 0;
  V sum_sqr_grad = 0;

  void get(char const* data, SGDState<V>* state) {
    // update model
    V grad = *((V*)data);
    sum_sqr_grad += grad * grad;

    V delta = state->lr.eta() * (grad / sqrt(sum_sqr_grad) + state->lambda * weight);
    delta = project(delta, state->max_delta);
    weight -= delta;

    // update status
    state->penalty -= 2.0 * weight * delta + delta * delta;
  }

  void put(char* data, SGDState<V>* state) {
    *((V*)data) = weight;
  }
};

template <typename V>
struct SGDEntry {
  V weight = 0;

  void get(char const* data, SGDState<V>* state) {
    // update model
    V grad = *((V*)data);
    V delta = state->lr.eta() * (grad + state->lambda * weight);
    delta = project(delta, state->max_delta);
    weight -= delta;

    // update status
    state->penalty -= 2.0 * weight * delta + delta * delta;
  }

  void put(char* data, SGDState<V>* state) {
    *((V*)data) = weight;
  }
};


template <typename V>
class AsyncSGDServer : public ISGDServer, public LinearMethod {
public:
  AsyncSGDServer(const string& name, const Config& conf)
      : ISGDServer(name), LinearMethod(conf) {
    if (conf_.sgd().ada_grad()) {
      model_ = CHECK_NOTNULL((
          new KVStore<Key, SGDEntry<V>, SGDState<V>>(name+"_model", name)));
    } else {
      model_ = CHECK_NOTNULL((
          new KVStore<Key, AdaGradEntry<V>, SGDState<V>>(name+"_model", name)));
    }
  }

  virtual ~AsyncSGDServer() {
    delete model_;
  }

  void init() {
    model_->setEntrySyncSize(sizeof(V));

    SGDState<V> state;
    auto sgd = conf_.sgd();
    // set learning rate and panlty
    state.lr = sgd.learning_rate();
    auto rg = conf_.penalty();
    if (rg.lambda_size() > 0) state.lambda = rg.lambda(0);
    model_->setState(state);

    // tail feature filter
    model_->setTailFilterSize(
        0, sgd.countmin_n()/sys_.yp().num_servers(), sgd.countmin_k());
  }

  void evaluate(SGDProgress* prog) {
    // auto s = model_.state();
    // prog->add_objective(s.norm1 * s.lambda1 + .5 * s.lambda2 * sqrt(s.norm2));
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
    const auto& sgd = conf_.sgd();
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

} // namespace LM
} // namespace PS
