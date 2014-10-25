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

    data_buf_.setMaxCapacity(conf_.solver().max_data_buf_size_in_mb() * 1000000);
  }
}

void FTRL::run() {
  // start the system
  LinearMethod::startSystem();

  // the thread collects progress from workers and servers
  prog_thr_ = unique_ptr<std::thread>(new std::thread([this]() {
        sleep(conf_.solver().eval_interval());
        while (true) {
          sleep(conf_.solver().eval_interval());
          showProgress();
        }
      }));
  prog_thr_->detach();

  // run in the eventual consistency model
  Task update = newTask(Call::UPDATE_MODEL);
  taskpool(kActiveGroup)->submitAndWait(update);

  Task save_model = newTask(Call::SAVE_MODEL);
  taskpool(kActiveGroup)->submitAndWait(save_model);
  // TODO collect progress

}

void FTRL::updateModel(const MessagePtr& msg) {

  // the thread reports progress to the scheduler
  prog_thr_ = unique_ptr<std::thread>(new std::thread([this]() {
        while (true) {
          sleep(conf_.solver().eval_interval());
          evalProgress();
        }
      }));
  prog_thr_->detach();

  if (IamServer()) return;

  // read data minibatches
  data_thr_ = unique_ptr<std::thread>(new std::thread([this]() {
        StreamReader<Real> reader(conf_.training_data());
        MatrixPtrList<Real> X;
        uint32 minibatch = conf_.solver().minibatch_size();
        while(!read_data_finished_) {
          bool ret = reader.readMatrices(minibatch, &X);
          CHECK_EQ(X.size(), 2);
          // LL << X[0]->debugString();
          // LL << X[1]->debugString();
          data_buf_.push(X, X[0]->memSize() + X[1]->memSize());
          if (!ret) read_data_finished_ = true;
        }
      }));
  data_thr_->detach();

  MatrixPtrList<Real> X;
  for (int i = 0; ; ++i) {
    if (read_data_finished_ && data_buf_.empty()) break;
    data_buf_.pop(X);

    SArray<Key> uniq_key;
    SArray<uint32> key_cnt;
    Localizer<Key, Real> localizer;
    localizer.countUniqIndex(X[1], &uniq_key, &key_cnt);

    // pull the working set
    MessagePtr filter(new Message(kServerGroup));
    filter->setKey(uniq_key);
    filter->addValue(key_cnt);
    filter->task.set_key_channel(i);
    filter->addFilter(FilterConfig::KEY_CACHING);
    filter->wait = true;
    auto arg = worker_w_->set(filter);
    arg->set_insert_key_freq(true);
    arg->set_query_key_freq(conf_.solver().tail_feature_freq());
    worker_w_->pull(filter);

    MessagePtr pull_val(new Message(kServerGroup));
    pull_val->setKey(worker_w_->key(i));
    pull_val->addFilter(FilterConfig::KEY_CACHING);
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
    CHECK_EQ(w.second.size(), 1);
    worker_w_->value(i) = w.second[0];
    // LL << w[0].second;

    SArray<Real> Xw(Y->rows());
    Xw.eigenArray() = *Z * worker_w_->value(i).eigenArray();
    Real objv = loss_->evaluate({Y, Xw.matrix()});
    {
      Lock l(status_mu_);
      status_.objv += objv;
      status_.num_ex += Xw.size();
    }

    SArray<Real> grad(Z->cols());
    loss_->compute({Y, Z, Xw.matrix()}, {grad.matrix()});
    // LL << grad;
    // push local gradient
    MessagePtr push_msg(new Message(kServerGroup));

    push_msg->setKey(worker_w_->key(i));
    push_msg->addValue({pos,neg});
    push_msg->addValue(grad);
    push_msg->task.set_key_channel(i);
    push_msg->addFilter(FilterConfig::KEY_CACHING)->set_clear_cache_if_done(true);
    // push_msg->task.set_erase_key_cache(true);
    time = worker_w_->push(push_msg);

    worker_w_->waitOutMsg(kServerGroup, time);
    worker_w_->clear(i);
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
  if (!IamServer()) return;
  if (!conf_.has_model_output()) return;
  auto out = ithFile(conf_.model_output(), 0, "_" + myNodeID());
  server_w_->writeToFile(out);
  LI << myNodeID() << " writes model to " << out.file(0);
}

void FTRL::evalProgress() {
  Progress prog;
  if (IamWorker()) {
    Lock l(status_mu_);
    prog.set_objv(status_.objv);
    prog.set_acc(status_.acc);
    prog.set_num_ex_trained(status_.num_ex);
    status_.reset();
  } else if (IamServer()) {
    prog.set_objv(server_w_->objv());
    prog.set_nnz_w(server_w_->nnz());
  }
  auto report = newTask(Call::REPORT_PROGRESS);
  string str; CHECK(prog.SerializeToString(&str));
  report.set_msg(str);
  taskpool(schedulerID())->submit(report);
}

void FTRL::showProgress() {
  Lock l(progress_mu_);
  Real objv_worker = 0, objv_server = 0;
  uint64 num_ex = 0, nnz_w = 0;
  for (const auto& it : recent_progress_) {
    auto prog = it.second;
    if (prog.has_num_ex_trained()) {
      num_ex += prog.num_ex_trained();
      objv_worker += prog.objv();
    } else {
      nnz_w += prog.nnz_w();
      objv_server += prog.objv();
    }
  }
  status_.num_ex += num_ex;
  printf("%10llu examples, loss %.5e, penalty %.5e, |w|_0 %8llu\n",
         status_.num_ex, objv_worker/(Real)num_ex, objv_server, nnz_w);
}

} // namespace LM
} // namespace PS
