#pragma once
#include <random>
#include "ps.h"
#include "learner/sgd.h"
#include "util/evaluation.h"
#include "parameter/kv_vector.h"
#include "parameter/kv_store.h"
#include "app/linear_method/learning_rate.h"
#include "app/linear_method/proto/linear.pb.h"
#include "app/linear_method/loss.h"
#include "app/linear_method/penalty.h"
namespace PS {
namespace LM {

// async sgd. support various loss functions and regularizers

class AsyncSGDScheduler : public ISGDScheduler {
 public:
  AsyncSGDScheduler(const Config& conf)
      : ISGDScheduler(), conf_(conf) {
    Workload load;
    *load.mutable_data() = conf_.training_data();
    load.mutable_data()->set_ignore_feature_group(true);
    load.set_replica(conf_.async_sgd().num_data_pass());
    load.set_shuffle(true);
    workload_pool_ = new WorkloadPool(load);
  }
  virtual ~AsyncSGDScheduler() { }

 private:
  Config conf_;
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
    if (!reporter) return;
    SGDProgress prog;
    prog.set_nnz(nnz);
    prog.set_weight_sum(weight_sum); weight_sum = 0;
    prog.set_delta_sum(delta_sum); delta_sum = 0;
    reporter->report(prog);
  }

  void updateWeight(V new_weight, V old_weight) {
    // LL << new_weight << " " << old_weight;
    if (new_weight == 0 && old_weight != 0) {
      -- nnz;
    } else if (new_weight != 0 && old_weight == 0) {
      ++ nnz;
    }
    weight_sum += new_weight * new_weight;
    V delta = new_weight - old_weight;
    delta_sum += delta * delta;
  }

  shared_ptr<LearningRate<V>> lr;
  shared_ptr<Penalty<V>> h;

  int iter = 0;
  size_t nnz = 0;
  V weight_sum = 0;
  V delta_sum = 0;
  V max_delta = 1.0;  // maximal change of weight
  MonitorSlaver<SGDProgress>* reporter = nullptr;
};

template <typename V>
struct AdaGradEntry {
  void get(V const* data, SGDState<V>* state) {
    // update model
    V grad = *data;
    sum_sq_grad += grad * grad;
    // state->update(weight, grad, sqrt(sum_sq_grad));
    // TODO
  }

  void put(V* data, SGDState<V>* state) {
    *data = weight;
  }
  V weight = 0;
  V sum_sq_grad = 0;
};

template <typename V>
struct SGDEntry {
  void get(V const* data, SGDState<V>* state) {
    // V grad = *((V*)data);
    // state->update(weight, grad);
    // TODO
  }
  void put(V* data, SGDState<V>* state) {
    *data = weight;
  }
  V weight = 0;
};

template <typename V>
struct FTRLEntry {
  V w = 0;  // not necessary to store w, because it can be computed from z
  V z = 0;
  V sqrt_n = 0;

  void get(V const* data, SGDState<V>* state) {
    // update model
    V w_old = w;
    V grad = *data;
    V sqrt_n_new = sqrt(sqrt_n * sqrt_n + grad * grad);
    V sigma = (sqrt_n_new - sqrt_n) / state->lr->alpha();
    z += grad  - sigma * w;
    sqrt_n = sqrt_n_new;

    V eta = state->lr->eval(sqrt_n);
    w = state->h->proximal(-z*eta, eta);
    state->updateWeight(w, w_old);
  }

  void put(V* data, SGDState<V>* state) {
    *data = w;
  }
};

template <typename V>
class AsyncSGDServer : public ISGDCompNode {
public:
  AsyncSGDServer(const Config& conf)
      : ISGDCompNode(), conf_(conf) {
    if (conf_.async_sgd().algo() == SGDConfig::FTRL) {
      model_ = new KVStore<Key, V, FTRLEntry<V>, SGDState<V>>();
    } else {
      if (conf_.async_sgd().ada_grad()) {
        model_ = new KVStore<Key, V, SGDEntry<V>, SGDState<V>>();
      } else {
        model_ = new KVStore<Key, V, AdaGradEntry<V>, SGDState<V>>();
      }
    }

    SGDState<V> state(conf_.penalty(), conf_.learning_rate());
    state.reporter = &(this->reporter_);
    CHECK_NOTNULL(model_)->setState(state);
  }

  virtual ~AsyncSGDServer() {
    delete model_;
  }

  void saveModel() {
    auto output = conf_.model_output();
    if (output.format() == DataConfig::TEXT) {
      CHECK(output.file_size());
      std::string file = output.file(0) + "_" + MyNodeID();
      CHECK_NOTNULL(model_)->writeToFile(file);
      LI << MyNodeID() << " written the model to " << file;
    }
  }

  virtual void process(const MessagePtr& msg) {
    if (msg->task.sgd().cmd() == SGDCall::SAVE_MODEL) {
      saveModel();
    }
  }
 protected:
  KVState<Key, SGDState<V>>* model_ = nullptr;
  Config conf_;
};

template <typename V>
class AsyncSGDWorker : public ISGDCompNode {
 public:
  AsyncSGDWorker(const Config& conf)
      : ISGDCompNode(), conf_(conf) {
    loss_ = createLoss<V>(conf_.loss());
  }
  virtual ~AsyncSGDWorker() { }

  virtual void process(const MessagePtr& msg) {
    const auto& sgd = msg->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      // do workload
      updateModel(sgd.load());

      // reply the scheduler with the finished id
      Task done;
      done.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
      done.mutable_sgd()->mutable_load()->add_finished(sgd.load().id());
      sys_.reply(msg, done);
      msg->replied = true;
    }
  }

  virtual void run() {
    WaitServersReady();

    // request workload from the scheduler
    Task task; task.mutable_sgd()->set_cmd(SGDCall::REQUEST_WORKLOAD);
    port(SchedulerID())->submit(task);
  }

 private:
  void updateModel(const Workload& load) {
    LOG(INFO) << MyNodeID() << ": accept workload " << load.id();
    VLOG(1) << "workload data: " << load.data().ShortDebugString();
    const auto& sgd = conf_.async_sgd();
    MinibatchReader<V> reader;
    reader.setReader(load.data(), sgd.minibatch(), sgd.data_buf());
    reader.setFilter(sgd.countmin_n(), sgd.countmin_k(), sgd.tail_feature_freq());
    reader.start();

    processed_batch_ = 0;
    int id = 0;
    SArray<Key> key;
    while (true) {
      mu_.lock();
      auto& data = data_[id];
      mu_.unlock();
      if (!reader.read(data.first, data.second, key)) break;
      VLOG(1) << "load minibatch " << id << ", X: "
              << data.second->rows() << "-by-" << data.second->cols();

      // pull the weight
      MessagePtr msg(new Message(kServerGroup));
      msg->setKey(key);
      msg->task.set_key_channel(id);
      msg->fin_handle = [this, id]() { computeGradient(id); };
      model_.key(id) = key;
      model_.pull(msg);

      ++ id;
    }

    while (processed_batch_ < id) { usleep(500); }
    LOG(INFO) << MyNodeID() << ": finished workload " << load.id();
  }

  void computeGradient(int id) {
    mu_.lock();
    auto Y = data_[id].first;
    auto X = data_[id].second;
    data_.erase(id);
    mu_.unlock();
    CHECK_EQ(X->rows(), Y->rows());
    VLOG(1) << "compute gradient for minibatch " << id;

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

    // compute the gradient
    SArray<V> grad(X->cols());
    loss_->compute({Y, X, Xw.matrix()}, {grad.matrix()});

    // push the gradient
    MessagePtr msg(new Message(kServerGroup));
    msg->setKey(model_.key(id));
    msg->addValue(grad);
    msg->task.set_key_channel(id);
    msg->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    int nbytes = conf_.async_sgd().fixing_float_by_nbytes();
    if (nbytes) {
      auto conf = msg->addFilter(FilterConfig::FIXING_FLOAT)->add_fixed_point();
      conf->set_num_bytes(nbytes);
    }
    model_.push(msg);
    model_.clear(id);

    ++ processed_batch_;
  }

private:
  KVVector<Key, V> model_;
  LossPtr<V> loss_;

  // minibatch_id, Y, X
  std::unordered_map<int, std::pair<MatrixPtr<V>, MatrixPtr<V>>> data_;

  std::mutex mu_;
  std::atomic_int processed_batch_;
  int workload_id_ = -1;

  Config conf_;
};

} // namespace LM
} // namespace PS

// auto we = w.eigenArray();
// auto ge = grad.eigenArray();
// LL << we.minCoeff() << " " << we.maxCoeff() << " "
//    << w.mean() << " " << w.std() << " "
//    << ge.minCoeff() << " " << ge.maxCoeff() << " "
//    << grad.mean() << " " << grad.std();

// // project value into [-bound, bound]
// template <typename V>
// V project(V value, V bound) {
//   return (value > bound ? bound : (value < - bound ? - bound : value));
// }


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
