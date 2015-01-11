#pragma once
#include "learner/sgd.h"
#include "data/stream_reader.h"
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
struct FTRLEntry {
  V w = 0;  // not necessary to store w, because it can be computed from z
  V z = 0;
  V sqrt_n = 0;

  // learning rate
  static V alpha, beta;

  // penalty
  static V lambda1, lambda2;

  // status
  static V norm1;
  static V norm2;
  static size_t nnz;

  void get(char const* data) {
    // update model
    V w_old = w;
    V grad = *((V*)data);
    V sqrt_n_new = sqrt(sqrt_n * sqrt_n + grad * grad);
    V sigma = (sqrt_n_new - sqrt_n) / alpha;
    z += grad  - sigma * w;
    sqrt_n = sqrt_n_new;
    w = - softThresholding(
        z, lambda1, lambda2 + (beta + sqrt_n_new) / alpha);

    // update status
    norm1 += fabs(w) - fabs(w_old);
    norm2 += w * w - w_old * w_old;
    if (w == 0 && w_old != 0) {
      -- nnz;
    } else if (w != 0 && w_old == 0) {
      ++ nnz;
    }
  }

  void put(char* data) {
    *((V*)data) = w;
  }
};

template <typename V>
using FTRLModel = KVStore<Key, FTRLEntry<V>>;

template <typename V>
class FTRLServer : public SGDServer<FTRLModel<V>> {
public:
  FTRLServer(const string& name) : SGDServer<FTRLModel<V>>(name) { }
  virtual ~FTRLServer() { }

  typedef FTRLEntry<V> E;
  void init() {
    // init static variables
    E::alpha = 1;
  }

  void evaluate(SGDProgress* prog) {
    // prog->set_objv(E::norm1 * E::lambda1 + .5 * E::lambda2 * sqrt(E::norm2));
    // prog->set_nnz_w(E::nnz);
  }

  void saveModel() {
  }
};

template <typename V>
using FTRLWorkerBase =
    SGDWorker<StreamReader<V>, SparseMinibatch<V>, KVVector<Key, V>>;

template <typename V>
class FTRLWorker : public FTRLWorkerBase<V> {
 public:
  FTRLWorker(const string& name) : FTRLWorkerBase<V>(name) { }
  virtual ~FTRLWorker() { }
  bool readMinibatch(StreamReader<V>& reader, SparseMinibatch<V>* data) {
    // read a minibatch
    MatrixPtrList<V> ins;
    bool ret = reader.readMatrices(1000, &ins);
    // CHECK_EQ(ins.size(), 2);
    // // LL << ins[0]->debugString() << "\n" << ins[1]->debugString();
    // data->label = ins[0];

    // // find all unique features,
    // SArray<Key> uniq_key;
    // SArray<uint8> key_cnt;
    // data->localizer = LocalizerPtr<Key, V>(new Localizer<Key, V>());
    // data->localizer->countUniqIndex(ins[1], &uniq_key, &key_cnt);

    // // pull the features and weights from servers with tails filtered
    // MessagePtr msg(new Message(kServerGroup));
    // msg->task.set_key_channel(batch_id);
    // msg->setKey(uniq_key);
    // msg->addValue(key_cnt);
    // msg->addFilter(FilterConfig::KEY_CACHING);
    // auto tail = model_.set(msg)->mutable_tail_filter();
    // tail->set_insert_count(true);
    // tail->set_query_key(conf_.solver().tail_feature_freq());
    // tail->set_query_value(true);
    // data->pull_time = model_.pull(msg);

    // data->batch_id = batch_id ++;
    return ret;
  }
  void computeGradient(SparseMinibatch<V>& data) {
    // // release some memory
    // int id = batch.batch_id;
    // if (pre_batch >= 0) {
    //   model_.clear(pre_batch);
    //   pre_batch = id;
    // }
    // // waiting the model working set
    // model_.waitOutMsg(kServerGroup, batch.pull_time);

    // // localize the feature matrix
    // auto X = batch.localizer->remapIndex(model_.key(id));
    // auto Y = batch.label;
    // CHECK_EQ(X->rows(), Y->rows());

    // // compute the gradient
    // SArray<real> Xw(Y->rows());
    // auto w = model_.value(id);
    // Xw.eigenArray() = *X * w.eigenArray();
    // real objv = loss_->evaluate({Y, Xw.matrix()});
    // real auc = Evaluation<real>::auc(Y->value(), Xw);
    // // not with penalty.
    // // penalty_->evaluate(w.matrix());
    // {
    //   Lock l(prog_mu_);
    //   prog_.add_objv(objv);
    //   prog_.add_auc(auc);
    //   prog_.set_num_ex_trained(prog_.num_ex_trained() + Xw.size());
    // }
    // SArray<real> grad(X->cols());
    // loss_->compute({Y, X, Xw.matrix()}, {grad.matrix()});

    // // push the gradient
    // MessagePtr msg(new Message(kServerGroup));
    // msg->setKey(model_.key(i));
    // msg->addValue(grad);
    // msg->task.set_key_channel(id);
    // // msg->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    // model_.push(msg);
  }
private:
  int batch_id_ = 0;

};

class FTRLScheduler : public SGDScheduler {
 public:
  FTRLScheduler(const string& name) : SGDScheduler(name) { }
  virtual ~FTRLScheduler() { }
};

} // namespace LM
} // namespace PS
