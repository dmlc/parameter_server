#include "linear_method/ftrl.h"
#include "linear_method/ftrl_model.h"

namespace PS {
namespace LM {

void FTRL::init() {
  OnlineSolver::init();
 if (IamServer()) {
    model_ = SharedParameterPtr<Key>(new FTRLModel<Key, Real>());
    model_->keyFilter(0).resize(
        conf_.solver().countmin_n()/FLAGS_num_servers, conf_.solver().countmin_k());
    model_->setKeyFilterIgnoreChl(true);
    model_->setLearningRate(conf_.learning_rate());
  } else if (IamWorker()) {
    model_ = SharedParameterPtr<Key>(new KVVector<Key, Real>());
    loss_ = Loss<Real>::create(conf_.loss());
  }
  model_->name() = app_cf_.parameter_name(0);
  sys_.yp().add(std::static_pointer_cast<Customer>(model_));
}

void FTRL::run() {
  // start the system
  LinearMethod::startSystem();

  // run in the eventual consistency model
  Task update = newTask(Call::UPDATE_MODEL);
  taskpool(kActiveGroup)->submit(update);

  // TODO collect progress

}

void FTRL::updateModel(const MessagePtr& msg) {

  // int time = msg->task.time() * 10;
  if (IamWorker()) {

    StreamReader<Real> reader(conf_.training_data());
    MatrixPtrList<Real> X;
    uint32 minibatch = conf_.solver().minibatch_size();

    for (int i = 0; reader.readMatrices(minibatch, &X); ++i) {
      // read a minibatch
      SArray<Key> uniq_key;
      SArray<uint32> key_cnt;
      Localizer<Key, Real> localizer;
      localizer.countUniqIndex(X[1], &uniq_key, &key_cnt);

      // pull the working set
      MessagePtr filter(new Message(kServerGroup));
      filter->addKV(uniq_key, {key_cnt});
      filter->task.set_key_channel(i);
      auto arg = model_->set(filter);
      arg->set_insert_key_freq(true);
      arg->set_query_key_freq(conf_.solver().tail_feature_freq());
      int time = model_->pull(filter);

      model_->waitOutMsg(kServerGroup, time);
      auto Z = localizer.remapIndex(model_->key(i));
      auto Y = X[0];
      CHECK_EQ(Z->rows(), Y->rows());

      // compute local gradient
      SArray<uint32> pos, neg;
      countKeys(Y, Z, &pos, &neg);

      SArray<Real> Xw(Y->rows());

      Xw.eigenArray() = *X * model_->value(i).eigenArray();
      Real objv = loss_->evalute({Y, Xw.matrix()});

      SArray<Real> grad;
      loss_->compute({Y, Z, Xw.matrix()}, {grad});

      // push local gradient
      MessagePtr push_msg(new Message(kServerGroup));

      push_msg->addKV(model_->key(i), {pos, neg});
      push_msg->addValue(grad);
      Range<Key>::all().to(push_msg->task.mutable_key_range());
      push_msg->task.set_key_channel(grp);
      push_msg->task.set_erase_key_cache(true);
      time = model_->push(push_msg);

      model_->waitOutMsg(kServerGroup, time);
    }
  } else if (IamServer()) {

  }
}

void FTRL::countKeys(const MatrixPtr<Real>& Y, const MatrixPtr<Real>& X,
                     SArray<uint32>* pos, SArray<uint32>* neg) {
  CHECK(X->rowMajor());
  auto SX = std::static_pointer_cast<SparseMatrix<uint32, Real>>(X);
  SArray<size_t> os = SX->offset();
  SArray<int32> idx = SX->index();
  CHECK_EQ(os.back(), idx.size());

  SArray<Real> y = Y->value();

  int p = X->cols();
  pos->resize(p); pos->setZero();
  neg->resize(p); neg->setZero();
  for (int i = 0; i < os.size()-1; ++i) {
    if (y[i] > 0) {
      for (size_t j = os[i]; j < os[i+1]; ++j) ++pos[idx[j]];
    } else {
      for (size_t j = os[i]; j < os[i+1]; ++j) ++neg[idx[j]];
    }
  }
}

void FTRL::saveModel(const MessageCPtr& msg) {
  // TODO
}

} // namespace LM
} // namespace PS
