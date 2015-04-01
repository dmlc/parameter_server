/**
 * @file   async_sgd.h
 * @brief  Asynchronous stochastic gradient descent to solve linear methods.
 */
#pragma once
#include <random>
#include "ps.h"
#include "learner/sgd.h"
#include "util/evaluation.h"
#include "parameter/kv_vector.h"
#include "parameter/kv_map.h"
#include "app/linear_method/learning_rate.h"
#include "app/linear_method/proto/linear.pb.h"
#include "app/linear_method/loss.h"
#include "app/linear_method/penalty.h"
namespace PS {
namespace LM {

/**
 * @brief The scheduler node
 */
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

/**
 * @brief A server node
 */
template <typename V>
class AsyncSGDServer : public ISGDCompNode {
 public:
  AsyncSGDServer(const Config& conf)
      : ISGDCompNode(), conf_(conf) {
    SGDState state(conf_.penalty(), conf_.learning_rate());
    state.reporter = &(this->reporter_);
    if (conf_.async_sgd().algo() == SGDConfig::FTRL) {
      auto model = new KVMap<Key, V, FTRLEntry, SGDState>();
      model->set_state(state);
      model_ = model;
    } else {
      // if (conf_.async_sgd().ada_grad()) {
      //   model_ = new KVStore<Key, V, SGDEntry<V>, SGDState<V>>();
      // } else {
      //   model_ = new KVStore<Key, V, AdaGradEntry<V>, SGDState<V>>();
      // }
    }
  }

  virtual ~AsyncSGDServer() {
    delete model_;
  }

  void SaveModel() {
    auto output = conf_.model_output();
    if (output.format() == DataConfig::TEXT) {
      CHECK(output.file_size());
      std::string file = output.file(0) + "_" + MyNodeID();
      // CHECK_NOTNULL(model_)->writeToFile(file);
      // LI << MyNodeID() << " written the model to " << file;
    }
  }

  virtual void ProcessRequest(Message* request) {
    if (request->task.sgd().cmd() == SGDCall::SAVE_MODEL) {
      SaveModel();
    }
  }
 protected:
  Parameter* model_ = nullptr;
  Config conf_;

  /**
   * @brief Progress state
   */
  struct SGDState {
    SGDState() { }
    SGDState(const PenaltyConfig& h_conf, const LearningRateConfig& lr_conf) {
      lr = std::shared_ptr<LearningRate<V>>(new LearningRate<V>(lr_conf));
      h = std::shared_ptr<Penalty<V>>(createPenalty<V>(h_conf));
    }
    virtual ~SGDState() { }

    void Update() {
      if (!reporter) return;
      SGDProgress prog;
      prog.set_nnz(nnz);
      prog.set_weight_sum(weight_sum); weight_sum = 0;
      prog.set_delta_sum(delta_sum); delta_sum = 0;
      reporter->Report(prog);
    }

    void UpdateWeight(V new_weight, V old_weight) {
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

    std::shared_ptr<LearningRate<V>> lr;
    std::shared_ptr<Penalty<V>> h;

    int iter = 0;
    size_t nnz = 0;
    V weight_sum = 0;
    V delta_sum = 0;
    V max_delta = 1.0;  // maximal change of weight
    MonitorSlaver<SGDProgress>* reporter = nullptr;
  };

  /**
   * @brief An entry for FTRL
   */
  struct FTRLEntry {
    V w = 0;  // not necessary to store w, because it can be computed from z
    V z = 0;
    V sqrt_n = 0;

    void Set(const V* data, void* state) {
      SGDState* st = (SGDState*) state;
      // update model
      V w_old = w;
      V grad = *data;
      V sqrt_n_new = sqrt(sqrt_n * sqrt_n + grad * grad);
      V sigma = (sqrt_n_new - sqrt_n) / st->lr->alpha();
      z += grad  - sigma * w;
      sqrt_n = sqrt_n_new;

      // update status
      V eta = st->lr->eval(sqrt_n);
      w = st->h->proximal(-z*eta, eta);
      st->UpdateWeight(w, w_old);
    }

    void Get(V* data, void* state) { *data = w; }
  };

};

/**
 * @brief A worker node
 */
template <typename V>
class AsyncSGDWorker : public ISGDCompNode {
 public:
  AsyncSGDWorker(const Config& conf)
      : ISGDCompNode(), conf_(conf) {
    loss_ = createLoss<V>(conf_.loss());
  }
  virtual ~AsyncSGDWorker() { }

  virtual void ProcessRequest(Message* request) {
    const auto& sgd = request->task.sgd();
    if (sgd.cmd() == SGDCall::UPDATE_MODEL) {
      // do workload
      UpdateModel(sgd.load());

      // reply the scheduler with the finished id
      Task done;
      done.mutable_sgd()->set_cmd(SGDCall::UPDATE_MODEL);
      done.mutable_sgd()->mutable_load()->add_finished(sgd.load().id());
      Reply(request, done);
    }
  }

  virtual void Run() {
    // request workload from the scheduler
    Task task;
    task.mutable_sgd()->set_cmd(SGDCall::REQUEST_WORKLOAD);
    Submit(task, SchedulerID());
  }

 private:
  /**
   * @brief Process a file
   *
   * @param load
   */
  void UpdateModel(const Workload& load) {
    LOG(INFO) << MyNodeID() << ": accept workload " << load.id();
    VLOG(1) << "workload data: " << load.data().ShortDebugString();
    const auto& sgd = conf_.async_sgd();
    MinibatchReader<V> reader;
    reader.InitReader(load.data(), sgd.minibatch(), sgd.data_buf());
    reader.InitFilter(sgd.countmin_n(), sgd.countmin_k(), sgd.tail_feature_freq());
    reader.Start();

    processed_batch_ = 0;
    int id = 0;
    SArray<Key> key;
    for (; ; ++id) {
      mu_.lock();
      auto& data = data_[id];
      mu_.unlock();
      if (!reader.Read(data.first, data.second, key)) break;
      VLOG(1) << "load minibatch " << id << ", X: "
              << data.second->rows() << "-by-" << data.second->cols();

      // pull the weight
      auto req = Parameter::Request(id, -1, {}, sgd.pull_filter());
      model_[id].key = key;
      model_.Pull(req, key, [this, id]() { ComputeGradient(id); });
    }

    while (processed_batch_ < id) { usleep(500); }
    LOG(INFO) << MyNodeID() << ": finished workload " << load.id();
  }

  /**
   * @brief Compute gradient
   *
   * @param id minibatch id
   */
  void ComputeGradient(int id) {
    mu_.lock();
    auto Y = data_[id].first;
    auto X = data_[id].second;
    data_.erase(id);
    mu_.unlock();
    CHECK_EQ(X->rows(), Y->rows());
    VLOG(1) << "compute gradient for minibatch " << id;

    // evaluate
    SArray<V> Xw(Y->rows());
    auto w = model_[id].value;
    Xw.EigenArray() = *X * w.EigenArray();
    SGDProgress prog;
    prog.add_objective(loss_->evaluate({Y, Xw.SMatrix()}));
    // not with penalty. penalty_->evaluate(w.SMatrix());
    prog.add_auc(Evaluation<V>::auc(Y->value(), Xw));
    prog.add_accuracy(Evaluation<V>::accuracy(Y->value(), Xw));
    prog.set_num_examples_processed(
        prog.num_examples_processed() + Xw.size());
    this->reporter_.Report(prog);

    // compute the gradient
    SArray<V> grad(X->cols());
    loss_->compute({Y, X, Xw.SMatrix()}, {grad.SMatrix()});

    // push the gradient
    auto req = Parameter::Request(id, -1, {}, conf_.async_sgd().push_filter());
    model_.Push(req, model_[id].key, {grad});
    model_.Clear(id);

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

// template <typename V>
// struct AdaGradEntry {
//   void get(V const* data, SGDState<V>* state) {
//     // update model
//     V grad = *data;
//     sum_sq_grad += grad * grad;
//     // state->update(weight, grad, sqrt(sum_sq_grad));
//     // TODO
//   }

//   void put(V* data, SGDState<V>* state) {
//     *data = weight;
//   }
//   V weight = 0;
//   V sum_sq_grad = 0;
// };

// template <typename V>
// struct SGDEntry {
//   void get(V const* data, SGDState<V>* state) {
//     // V grad = *((V*)data);
//     // state->update(weight, grad);
//     // TODO
//   }
//   void put(V* data, SGDState<V>* state) {
//     *data = weight;
//   }
//   V weight = 0;
// };
