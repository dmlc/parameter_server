#include "linear_method/ftrl_worker.h"
#include "data/stream_reader.h"
#include "base/localizer.h"
namespace PS {
namespace LM {

void FTRLWorker::init(const string& name, const Config& conf) {
  conf_ = conf;
  loss_ = Loss<real>::create(conf_.loss());
  data_prefetcher_.setCapacity(conf_.solver().max_data_buf_size_in_mb());

  model_ = KVVectorPtr<Key, real>(new KVVector<Key, real>());
  model_->name() = name;
  Postoffice::instance().yp().add(std::static_pointer_cast<Customer>(model_));
}

void FTRLWorker::computeGradient() {

  // start the data prefectcher thread
  StreamReader<real> reader(conf_.training_data());
  int batch_id = 0;
  data_prefetcher_.startProducer(
      [this, &batch_id, &reader](Minibatch* data, size_t* size)->bool {
        // read a minibatch
        MatrixPtrList<real> ins;
        bool ret = reader.readMatrices(conf_.solver().minibatch_size(), &ins);
        CHECK_EQ(ins.size(), 2);
        data->label = ins[0];

        // find all unique features,
        SArray<Key> uniq_key;
        SArray<uint32> key_cnt;
        data->localizer.countUniqIndex(ins[1], &uniq_key, &key_cnt);
        // LL << ins[0]->debugString() << "\n" << ins[1]->debugString();

        // pull the features and weights from servers with tails filtered
        MessagePtr msg(new Message(kServerGroup));
        msg->task.set_key_channel(batch_id);
        msg->setKey(uniq_key);
        msg->addValue(key_cnt);
        // msg->addFilter(FilterConfig::KEY_CACHING);
        auto arg = model_->set(msg);
        arg->set_insert_key_freq(true);
        arg->set_query_key_freq(conf_.solver().tail_feature_freq());
        data->pull_time = model_->pull(msg);

        data->batch_id = batch_id ++;
        *size = data->label->memSize() + data->localizer.memSize();
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
    auto X = batch.localizer.remapIndex(model_->key(id));
    auto Y = batch.label;
    CHECK_EQ(X->rows(), Y->rows());

    // compute the gradient
    SArray<real> Xw(Y->rows());
    Xw.eigenArray() = *X * model_->value(id).eigenArray();
    real objv = loss_->evaluate({Y, Xw.matrix()});
    {
      Lock l(status_mu_);
      status_.objv += objv;
      status_.num_ex += Xw.size();
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
  Lock l(status_mu_);
  prog->set_objv(status_.objv);
  prog->set_acc(status_.acc);
  prog->set_num_ex_trained(status_.num_ex);
  status_.reset();
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
