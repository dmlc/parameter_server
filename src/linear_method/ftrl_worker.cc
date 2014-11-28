#include "linear_method/ftrl_worker.h"
#include "data/stream_reader.h"
#include "base/localizer.h"
#include "base/evaluation.h"
namespace PS {
namespace LM {

void FTRLWorker::init() {
  CompNode::init();
  data_prefetcher_.setCapacity(conf_.solver().max_data_buf_size_in_mb());
  model_ = KVVectorPtr<Key, real>(new KVVector<Key, real>());
  REGISTER_CUSTOMER(app_cf_.parameter_name(0), model_);
}

void FTRLWorker::computeGradient() {
  // start the data prefectcher thread
  // LL << conf_.training_data().DebugString();
  StreamReader<real> reader(conf_.training_data());
  int batch_id = 0;
  data_prefetcher_.startProducer(
      [this, &batch_id, &reader](Minibatch* data, size_t* size)->bool {
        // read a minibatch
        MatrixPtrList<real> ins;
        bool ret = reader.readMatrices(conf_.solver().minibatch_size(), &ins);
        CHECK_EQ(ins.size(), 2);
        // LL << ins[0]->debugString() << "\n" << ins[1]->debugString();
        data->label = ins[0];

        // find all unique features,
        SArray<Key> uniq_key;
        SArray<uint8> key_cnt;
        data->localizer = LocalizerPtr<Key, real>(new Localizer<Key, real>());
        data->localizer->countUniqIndex(ins[1], &uniq_key, &key_cnt);

        // pull the features and weights from servers with tails filtered
        MessagePtr msg(new Message(kServerGroup));
        msg->task.set_key_channel(batch_id);
        msg->setKey(uniq_key);
        msg->addValue(key_cnt);
        // msg->addFilter(FilterConfig::KEY_CACHING);
        auto tail = model_->set(msg)->mutable_tail_filter();
        tail->set_insert_count(true);
        tail->set_query_key(conf_.solver().tail_feature_freq());
        tail->set_query_value(true);
        data->pull_time = model_->pull(msg);

        data->batch_id = batch_id ++;
        *size = data->label->memSize() + data->localizer->memSize();
        return ret;
      });


  int pre_batch = -1;
  Minibatch batch;
  for (int i = 0; data_prefetcher_.pop(&batch); ++i) {
    // release some memory
    int id = batch.batch_id;
    if (pre_batch >= 0) {
      model_->clear(pre_batch);
      pre_batch = id;
    }
    // waiting the model working set
    model_->waitOutMsg(kServerGroup, batch.pull_time);

    // localize the feature matrix
    auto X = batch.localizer->remapIndex(model_->key(id));
    auto Y = batch.label;
    CHECK_EQ(X->rows(), Y->rows());

    // compute the gradient
    SArray<real> Xw(Y->rows());
    auto w = model_->value(id);
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
    msg->setKey(model_->key(i));
    msg->addValue(grad);
    msg->task.set_key_channel(id);
    // msg->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    model_->push(msg);
  }
}


void FTRLWorker::evaluateProgress(Progress* prog) {
  Lock l(prog_mu_);
  *prog = prog_;
  prog_.Clear();
}

} // namespace LM
} // namespace PS

// void FTRLWorker::countFrequency(
//     const MatrixPtr<real>& Y, const MatrixPtr<real>& X,
//     SArray<uint32>* pos, SArray<uint32>* neg) {
//   CHECK(X->rowMajor());
//   auto SX = std::static_pointer_cast<SparseMatrix<uint32, real>>(X);
//   SArray<size_t> os = SX->offset();
//   SArray<uint32> idx = SX->index();
//   CHECK_EQ(os.back(), idx.size());

//   SArray<real> y = Y->value();

//   int p = X->cols();
//   pos->resize(p); pos->setZero();
//   neg->resize(p); neg->setZero();
//   for (int i = 0; i < os.size()-1; ++i) {
//     if (y[i] > 0) {
//       for (size_t j = os[i]; j < os[i+1]; ++j) ++(*pos)[idx[j]];
//     } else {
//       for (size_t j = os[i]; j < os[i+1]; ++j) ++(*neg)[idx[j]];
//     }
//   }
// }
