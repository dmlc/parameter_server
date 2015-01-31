#pragma once
#include <random>
#include "learner/sgd.h"
#include "util/evaluation.h"
#include "parameter/kv_vector.h"
#include "parameter/kv_store.h"
#include "app/linear_method/learning_rate.h"
namespace PS {
namespace LM {

// // project value into [-bound, bound]
// template <typename V>
// V project(V value, V bound) {
//   return (value > bound ? bound : (value < - bound ? - bound : value));
// }

// async sgd. support various loss functions and regularizers
class AsyncSGDScheduler : public ISGDScheduler, public LinearMethod {
 public:
  AsyncSGDScheduler(const string& name, const Config& conf)
      : ISGDScheduler(name), LinearMethod(conf) { }
  virtual ~AsyncSGDScheduler() { }

  virtual void run() {
    updateModel(conf_.training_data(), conf_.async_sgd().report_interval());
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

  void update() {
    if (reporter) {
      SGDProgress prog; prog.set_nnz(nnz);
      reporter->report(prog);
    }
  }
  void updateWeight(V new_weight, V old_weight) {
    // LL << new_weight << " " << old_weight;
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
  MonitorSlaver<SGDProgress>* reporter = nullptr;
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
    // V grad = *((V*)data);
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
    state->updateWeight(w, w_old);
  }

  void put(char* data, SGDState<V>* state) {
    *((V*)data) = w;
  }
};



template <typename V>
class AsyncSGDServer : public ISGDCompNode, public LinearMethod {
public:
  AsyncSGDServer(const string& name, const Config& conf)
      : ISGDCompNode(name), LinearMethod(conf) {
    if (conf_.async_sgd().algo() == SGDConfig::FTRL) {
      model_ = new KVStore<Key, FTRLEntry<V>, SGDState<V>>(name+"_model", name);
    } else {
      if (conf_.async_sgd().ada_grad()) {
        model_ = new KVStore<Key, SGDEntry<V>, SGDState<V>>(name+"_model", name);
      } else {
        model_ = new KVStore<Key, AdaGradEntry<V>, SGDState<V>>(name+"_model", name);
      }
    }
  }

  virtual ~AsyncSGDServer() {
    delete model_;
  }

  void init() {
    CHECK_NOTNULL(model_)->setEntrySyncSize(sizeof(V));

    SGDState<V> state(conf_.penalty(), conf_.learning_rate());
    state.reporter = &(this->reporter_);
    model_->setState(state);

    // tail feature filter
    model_->setTailFilterSize(
        0, conf_.async_sgd().countmin_n()/sys_.yp().num_servers(),
        conf_.async_sgd().countmin_k());
  }


  void saveModel() {
    auto output = conf_.model_output();
    if (output.format() == DataConfig::TEXT) {
      CHECK(output.file_size());
      std::string file = output.file(0) + "_" + myNodeID();
      if (!dirExists(getPath(file))) {
        createDir(getPath(file));
      }
      std::ofstream out(file); CHECK(out.good());
      CHECK_NOTNULL(model_)->writeToFile([&out](const Key& key, char const* val) {
          V v = *((V const *)val);
          if (v != 0) out << key << "\t" << v << std::endl;
        });
      LI << myNodeID() << " written the model to " << file;
    }
  }

  virtual void process(const MessagePtr& msg) {
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::SAVE_MODEL) {
      saveModel();
    }
  }
 protected:
  KVState<Key, SGDState<V>>* model_ = nullptr;
};

template <typename V>
class AsyncSGDWorker : public ISGDCompNode, public LinearMethod {
 public:
  AsyncSGDWorker(const string& name, const Config& conf)
      : ISGDCompNode(name), LinearMethod(conf), model_(name+"_model", name) {
    loss_ = createLoss<V>(conf_.loss());
  }
  virtual ~AsyncSGDWorker() { }

  virtual void process(const MessagePtr& msg) {
    auto sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      updateModel(sgd);
    }
  }

  void updateModel(const SGDCall& call) {
    const auto& sgd = conf_.async_sgd();
    MinibatchReader<V> reader;
    reader.setReader(call.data(), sgd.minibatch(), sgd.data_buf());
    reader.setFilter(sgd.countmin_n(), sgd.countmin_k(), sgd.tail_feature_freq());
    reader.start();

    int id = 0;
    SArray<Key> key;
    while (true) {
      mu_.lock();
      auto& data = data_[id];
      mu_.unlock();
      if (!reader.read(data.first, data.second, key)) break;

      // pull the weight
      MessagePtr msg(new Message(kServerGroup));
      msg->setKey(key);
      msg->addFilter(FilterConfig::KEY_CACHING);
      msg->task.set_key_channel(id);
      msg->fin_handle = [this, id]() { computeGradient(id); };
      model_.key(id) = key;
      model_.pull(msg);

      ++ id;
    }
  }

  void computeGradient(int id) {
    mu_.lock();
    auto Y = data_[id].first;
    auto X = data_[id].second;
    data_.erase(id);
    mu_.unlock();
    CHECK_EQ(X->rows(), Y->rows());

    // evaluate
    SArray<V> Xw(Y->rows());
    auto w = model_.value(id);
    Xw.eigenArray() = *X * w.eigenArray();
    SGDProgress prog;
    prog.add_objective(loss_->evaluate({Y, Xw.matrix()}));
    // not with penalty.
    // + penalty_->evaluate(w.matrix());
    prog.add_auc(Evaluation<V>::auc(Y->value(), Xw));
    prog.add_accuracy(Evaluation<V>::accuracy(Y->value(), Xw));
    prog.set_num_examples_processed(
        prog.num_examples_processed() + Xw.size());
    this->reporter_.report(prog);
    // LL << prog.objective(0) << " " << prog.auc(0);

    // compute the gradient
    SArray<V> grad(X->cols());
    loss_->compute({Y, X, Xw.matrix()}, {grad.matrix()});

    // push the gradient
    MessagePtr msg(new Message(kServerGroup));
    msg->setKey(model_.key(id));
    msg->addValue(grad);
    msg->task.set_key_channel(id);
    msg->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    model_.push(msg);
    model_.clear(id);
  }

private:
  KVVector<Key, V> model_;
  LossPtr<V> loss_;

  std::unordered_map<int, std::pair<MatrixPtr<V>, MatrixPtr<V>>> data_;

  std::mutex mu_;
  // std::unordered_map<int, int> push_time_;

};

} // namespace LM
} // namespace PS

    // V n1 = 0, n2 = 0;
    // // add noise to gradient
    // V std = (V)conf_.async_sgd().noise_std();
    // if (std > 0) {
    //   std::default_random_engine generator;
    //   std::normal_distribution<V> distribution(0, std);
    //   for (auto& g : grad) {
    //     n1 += g * g;
    //     V s =  distribution(generator);
    //     n2 += s * s;
    //     g += s;
    //   }
    //   LL << sqrt(n1) << " " << sqrt(n2);
    // }
    // // LL <<  w.vec().norm() << " " << grad.vec().norm() << " " << auc << " " << objv;
