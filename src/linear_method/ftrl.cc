#include "linear_method/ftrl.h"
#include "linear_method/ftrl_model.h"
#include "data/stream_reader.h"
#include "base/localizer.h"
namespace PS {
namespace LM {

void FTRL::init() {
  OnlineSolver::init();
  if (IamServer()) {
    server_w_ = FTRLModelPtr<Key, Real>(new FTRLModel<Key, Real>());
    server_w_->setLearningRate(conf_.learning_rate());
    server_w_->setPenalty(conf_.penalty());
    server_w_->keyFilter(0).resize(
        conf_.solver().countmin_n()/FLAGS_num_servers, conf_.solver().countmin_k());
    server_w_->setKeyFilterIgnoreChl(true);

    server_w_->name() = app_cf_.parameter_name(0);
    sys_.yp().add(std::static_pointer_cast<Customer>(server_w_));
  } else if (IamWorker()) {
    worker_w_ = KVVectorPtr<Key, Real>(new KVVector<Key, Real>());
    loss_ = Loss<Real>::create(conf_.loss());

    worker_w_->name() = app_cf_.parameter_name(0);
    sys_.yp().add(std::static_pointer_cast<Customer>(worker_w_));
  }
}

void FTRL::run() {
  // start the system
  LinearMethod::startSystem();

  // run in the eventual consistency model
  Task update = newTask(Call::UPDATE_MODEL);
  taskpool(kActiveGroup)->submitAndWait(update);

  // TODO collect progress

}

void FTRL::updateModel(const MessagePtr& msg) {

  // int time = msg->task.time() * 10;
  if (IamWorker()) {

    StreamReader<Real> reader(conf_.training_data());
    MatrixPtrList<Real> X;
    uint32 minibatch = conf_.solver().minibatch_size();

    for (int i = 0; reader.readMatrices(minibatch, &X); ++i) {
      CHECK_EQ(X.size(), 2);
      // LL << X[0]->debugString();
      // LL << X[1]->debugString();

      // if (i > 1) break;
      // read a minibatch
      SArray<Key> uniq_key;
      SArray<uint32> key_cnt;
      Localizer<Key, Real> localizer;
      localizer.countUniqIndex(X[1], &uniq_key, &key_cnt);

      // pull the working set
      MessagePtr filter(new Message(kServerGroup));
      filter->addKV(uniq_key, {key_cnt});
      filter->task.set_key_channel(i);
      filter->wait = true;
      auto arg = worker_w_->set(filter);
      arg->set_insert_key_freq(true);
      arg->set_query_key_freq(conf_.solver().tail_feature_freq());
      worker_w_->pull(filter);

      MessagePtr pull_val(new Message(kServerGroup));
      pull_val->key = worker_w_->key(i);
      pull_val->task.set_key_channel(i);
      int time = worker_w_->pull(pull_val);

      auto Z = localizer.remapIndex(worker_w_->key(i));
      auto Y = X[0];
      CHECK_EQ(Z->rows(), Y->rows());

      // compute local gradient
      SArray<uint32> pos, neg;
      countKeys(Y, Z, &pos, &neg);


      worker_w_->waitOutMsg(kServerGroup, time);
      auto w = worker_w_->received(time);
      CHECK_EQ(w.size(), 1);
      worker_w_->value(i) = w[0].second;
      // LL << w[0].second;

      SArray<Real> Xw(Y->rows());
      Xw.eigenArray() = *Z * worker_w_->value(i).eigenArray();
      Real objv = loss_->evaluate({Y, Xw.matrix()});
      LL << objv;

      SArray<Real> grad(Z->cols());
      loss_->compute({Y, Z, Xw.matrix()}, {grad.matrix()});
      // LL << grad;
      // push local gradient
      MessagePtr push_msg(new Message(kServerGroup));

      push_msg->addKV(worker_w_->key(i), {pos, neg});
      push_msg->addValue(grad);
      Range<Key>::all().to(push_msg->task.mutable_key_range());
      push_msg->task.set_key_channel(i);
      push_msg->task.set_erase_key_cache(true);
      time = worker_w_->push(push_msg);

      worker_w_->waitOutMsg(kServerGroup, time);
    }
  } else if (IamServer()) {

  }
}

void FTRL::countKeys(const MatrixPtr<Real>& Y, const MatrixPtr<Real>& X,
                     SArray<uint32>* pos, SArray<uint32>* neg) {
  CHECK(X->rowMajor());
  auto SX = std::static_pointer_cast<SparseMatrix<uint32, Real>>(X);
  SArray<size_t> os = SX->offset();
  SArray<uint32> idx = SX->index();
  CHECK_EQ(os.back(), idx.size());

  SArray<Real> y = Y->value();

  int p = X->cols();
  pos->resize(p); pos->setZero();
  neg->resize(p); neg->setZero();
  for (int i = 0; i < os.size()-1; ++i) {
    if (y[i] > 0) {
      for (size_t j = os[i]; j < os[i+1]; ++j) ++(*pos)[idx[j]];
    } else {
      for (size_t j = os[i]; j < os[i+1]; ++j) ++(*neg)[idx[j]];
    }
  }
}

void FTRL::saveModel(const MessageCPtr& msg) {
  // TODO
}

} // namespace LM
} // namespace PS
