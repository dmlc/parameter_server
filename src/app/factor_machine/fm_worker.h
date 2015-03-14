#pragma once
#include "factorization_machine/fm.h"
#include "learner/sgd.h"
#include "parameter/kv_vector.h"
#include "base/evaluation.h"
namespace PS {
namespace FM {

class FMWorker {
 public:
  virtual void init() {
    REGISTER_CHILD_CUSTOMER(name() + "_w", w_, name());
    REGISTER_CHILD_CUSTOMER(name() + "_v", V_, name());
    k_ = 1;
  }
  virtual bool readMinibatch(StreamReader<Real>& reader, Minibatch* data) {
    // read a minibatch
    MatrixPtrList<Real> ins;
    bool ret = reader.readMatrices(1000, &ins);
    CHECK_EQ(ins.size(), 2);
    // LL << ins[0]->debugString() << "\n" << ins[1]->debugString();
    data->label = ins[0];

    // find all unique features,
    SArray<Key> uniq_key;
    SArray<uint8> key_cnt;
    data->localizer = LocalizerPtr<Key, Real>(new Localizer<Key, Real>());
    data->localizer->countUniqIndex(ins[1], &uniq_key, &key_cnt);

    // pull the features and weights from servers with tails filtered
    MessagePtr msg(new Message(kServerGroup));
    msg->task.set_key_channel(batch_id_);
    msg->setKey(uniq_key);
    msg->addValue(key_cnt);
    msg->AddFilter(FilterConfig::KEY_CACHING);
    auto tail = w_->set(msg)->mutable_tail_filter();
    tail->set_insert_count(true);
    tail->set_query_key(10);
    tail->set_query_value(true);
    data->pull_time = w_->pull(msg);

    data->batch_id = batch_id_ ++;
    return ret;
  }
  virtual void computeGradient(Minibatch& data) {
    // release some memory
    int id = data.batch_id;
    if (pre_batch_id_ >= 0) {
      w_->clear(pre_batch_id_);
      pre_batch_id_ = id;
    }
    // waiting the model working set
    w_->waitOutMsg(kServerGroup, data.pull_time);

    // localize the feature matrix
    auto X = data.localizer->remapIndex(w_->key(id));
    SArray<Real> y = data.label->value();
    CHECK_EQ(X->rows(), y.size());

    SArray<Real> w = w_->value(id);
    SArray<Real> py(y.size());
    py.arr() = *X * w.arr();

    // progress
    Real objv = log( 1 + exp( - y.arr() * py.arr() )).sum();
    Real auc = Evaluation<Real>::auc(y, py);
    {
      Lock l(progress_mu_);
      progress_.add_objective(objv);
      progress_.add_auc(auc);
      progress_.set_num_examples_processed(
          progress_.num_examples_processed() + y.size());
    }


    // compute the gradient
    SArray<Real> q(y.size());
    q.arr() = - y.arr() / ( 1 + exp(y.arr() * py.arr()));

    SArray<Real> grad(X->cols()); grad.arr() = X->transTimes( q.arr());

    // push the gradient
    MessagePtr msg(new Message(kServerGroup));
    msg->setKey(w_->key(id));
    msg->addValue(grad);
    msg->task.set_key_channel(id);
    msg->AddFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    w_->push(msg);
  }
 protected:
  int k_;
  KVVectorPtr<Key, Real> w_, V_;
  int batch_id_ = 0, pre_batch_id_ = -1;

};

} // namespace FM
} // namespace PS
