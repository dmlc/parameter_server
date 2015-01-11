#pragma once
#include "learner/sgd_worker.h"
#include "data/stream_reader.h"
namespace PS {
namespace LM {

template <typename V>
class FTRLWorker : public SGDWorker<
  StreamReader<V>, SparseMinibatch<V>, KVVector<Key, V>>> {

  bool readMinibatch(StreamReader<V>& reader, SparseMinibatch<V>* data) {
    // read a minibatch
    MatrixPtrList<V> ins;
    bool ret = reader.readMatrices(1000, &ins);
    CHECK_EQ(ins.size(), 2);
    // LL << ins[0]->debugString() << "\n" << ins[1]->debugString();
    data->label = ins[0];

    // find all unique features,
    SArray<Key> uniq_key;
    SArray<uint8> key_cnt;
    data->localizer = LocalizerPtr<Key, V>(new Localizer<Key, V>());
    data->localizer->countUniqIndex(ins[1], &uniq_key, &key_cnt);

    // pull the features and weights from servers with tails filtered
    MessagePtr msg(new Message(kServerGroup));
    msg->task.set_key_channel(batch_id);
    msg->setKey(uniq_key);
    msg->addValue(key_cnt);
    msg->addFilter(FilterConfig::KEY_CACHING);
    auto tail = model_.set(msg)->mutable_tail_filter();
    tail->set_insert_count(true);
    tail->set_query_key(conf_.solver().tail_feature_freq());
    tail->set_query_value(true);
    data->pull_time = model_.pull(msg);

    data->batch_id = batch_id ++;
    return ret;
  }
  void computeGradient(SparseMinibatch<V>& data) {
    // release some memory
    int id = batch.batch_id;
    if (pre_batch >= 0) {
      model_.clear(pre_batch);
      pre_batch = id;
    }
    // waiting the model working set
    model_.waitOutMsg(kServerGroup, batch.pull_time);

    // localize the feature matrix
    auto X = batch.localizer->remapIndex(model_.key(id));
    auto Y = batch.label;
    CHECK_EQ(X->rows(), Y->rows());

    // compute the gradient
    SArray<real> Xw(Y->rows());
    auto w = model_.value(id);
    Xw.eigenArray() = *X * w.eigenArray();
    real objv = loss_->evaluate({Y, Xw.matrix()});
    real auc = Evaluation<real>::auc(Y->value(), Xw);
    // not with penalty.
    // penalty_->evaluate(w.matrix());
    {
      Lock l(prog_mu_);
      prog_.add_objv(objv);
      prog_.add_auc(auc);
      prog_.set_num_ex_trained(prog_.num_ex_trained() + Xw.size());
    }
    SArray<real> grad(X->cols());
    loss_->compute({Y, X, Xw.matrix()}, {grad.matrix()});

    // push the gradient
    MessagePtr msg(new Message(kServerGroup));
    msg->setKey(model_.key(i));
    msg->addValue(grad);
    msg->task.set_key_channel(id);
    // msg->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    model_.push(msg);
  }
private:
  int batch_id_ = 0;

};
} // namespace LM
} // namespace PS
